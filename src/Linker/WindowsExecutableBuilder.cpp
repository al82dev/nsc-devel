#include "nativesharp/Linker/WindowsExecutableBuilder.h"

#include "nativesharp/Linker/WindowsRuntimeImports.h"

#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/Linker/LldLinker.h"
#include "nativesharp/Runtime/WindowsRuntimeBuilder.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace nativesharp::linker
{
namespace
{
class TemporaryDirectory final
{
public:
    explicit TemporaryDirectory(std::filesystem::path path) noexcept
        : path_(std::move(path))
    {
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path& path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

std::unique_ptr<TemporaryDirectory> createTemporaryDirectory(std::string& error)
{
    error.clear();
    std::error_code filesystemError;
    const std::filesystem::path root = std::filesystem::temp_directory_path(filesystemError);
    if (filesystemError)
    {
        error = "could not resolve the system temporary directory: " + filesystemError.message();
        return nullptr;
    }

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (std::uint32_t attempt = 0; attempt < 100; ++attempt)
    {
        const std::filesystem::path candidate = root /
            ("nativesharp-stage20-" + std::to_string(stamp) + "-" + std::to_string(attempt));
        filesystemError.clear();
        if (std::filesystem::create_directory(candidate, filesystemError))
        {
            return std::make_unique<TemporaryDirectory>(candidate);
        }
        if (filesystemError && filesystemError != std::errc::file_exists)
        {
            error = "could not create a Stage 20 temporary directory: " + filesystemError.message();
            return nullptr;
        }
    }

    error = "could not allocate a unique Stage 20 temporary directory";
    return nullptr;
}

bool verifyObject(const std::filesystem::path& path, std::string_view label, std::string& error)
{
    llvm_backend::CoffObjectVerifier verifier;
    if (!verifier.verify(path))
    {
        error = std::string(label) + " COFF verification failed: " + verifier.error();
        return false;
    }
    return true;
}
}

bool WindowsExecutableBuilder::build(
    llvm_backend::LlvmModule& programModule,
    const std::filesystem::path& outputPath,
    std::string& error)
{
    return build(programModule, outputPath, llvm_backend::LlvmOptimizationLevel::O0, error);
}

bool WindowsExecutableBuilder::build(
    llvm_backend::LlvmModule& programModule,
    const std::filesystem::path& outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::string& error)
{
    return build(programModule, outputPath, optimizationLevel, "Main", error);
}

bool WindowsExecutableBuilder::build(
    llvm_backend::LlvmModule& programModule,
    const std::filesystem::path& outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::string_view mainSymbol,
    std::string& error)
{
    info_ = {};
    error.clear();
    if (outputPath.empty())
    {
        error = "Windows executable output path is empty";
        return false;
    }
    if (mainSymbol.empty())
    {
        error = "Windows executable entry symbol is empty";
        return false;
    }

    const std::filesystem::path parent = outputPath.parent_path();
    if (!parent.empty() && !std::filesystem::is_directory(parent))
    {
        error = "Windows executable output directory does not exist: " + parent.string();
        return false;
    }

    std::string targetError;
    std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine =
        llvm_backend::LlvmTargetMachine::createWindowsX64(optimizationLevel, targetError);
    if (targetMachine == nullptr)
    {
        error = "could not create the Windows x64 TargetMachine: " + targetError;
        return false;
    }

    runtime::WindowsRuntimeBuilder runtimeBuilder;
    std::string runtimeError;
    std::unique_ptr<llvm_backend::LlvmModule> runtimeModule = runtimeBuilder.build(mainSymbol, runtimeError);
    if (runtimeModule == nullptr)
    {
        error = "could not construct the NativeSharp runtime: " + runtimeError;
        return false;
    }

    llvm_backend::LlvmOptimizer optimizer;
    std::string optimizationError;
    if (!optimizer.optimize(programModule, *targetMachine, optimizationLevel, optimizationError))
    {
        error = "program LLVM optimization failed: " + optimizationError;
        return false;
    }
    optimizationError.clear();
    if (!optimizer.optimize(*runtimeModule, *targetMachine, optimizationLevel, optimizationError))
    {
        error = "runtime LLVM optimization failed: " + optimizationError;
        return false;
    }

    std::string temporaryError;
    std::unique_ptr<TemporaryDirectory> temporaryDirectory =
        createTemporaryDirectory(temporaryError);
    if (temporaryDirectory == nullptr)
    {
        error = temporaryError;
        return false;
    }

    const std::filesystem::path programObject = temporaryDirectory->path() / "program.obj";
    const std::filesystem::path runtimeObject = temporaryDirectory->path() / "runtime.obj";
    std::string emissionError;
    if (!targetMachine->emit(
            programModule,
            programObject,
            llvm_backend::LlvmCodeGenFileType::Object,
            emissionError))
    {
        error = "program object emission failed: " + emissionError;
        return false;
    }
    if (!verifyObject(programObject, "program", error))
    {
        return false;
    }

    emissionError.clear();
    if (!targetMachine->emit(
            *runtimeModule,
            runtimeObject,
            llvm_backend::LlvmCodeGenFileType::Object,
            emissionError))
    {
        error = "runtime object emission failed: " + emissionError;
        return false;
    }
    if (!verifyObject(runtimeObject, "runtime", error))
    {
        return false;
    }

    // Stage 53.2b bug fix: this scan MUST cover both modules that actually get linked into the
    // final executable, not just the user's program module. Before this fix, only programText
    // was scanned -- which missed the case (introduced by Environment.GetEnvironmentVariable/
    // SetEnvironmentVariable's hand-rolled byte-copy loop in WindowsRuntimeBuilder.cpp) where
    // LLVM's own LoopIdiomRecognize optimization pass rewrites a plain copy loop into a call to
    // the @llvm.memcpy intrinsic *inside the runtime module*, at any optimization level above
    // O0. That intrinsic call lowers to a real "memcpy" symbol reference in runtime.obj, and
    // without this fix nothing ever told the linker to import it -- "undefined symbol: memcpy"
    // at link time, for EVERY executable (not just ones that call GetEnvironmentVariable),
    // because every executable links against the same runtime module. Concatenating both
    // modules' IR text before scanning is the minimal, honest fix: it makes no assumption about
    // which module a future runtime helper's copy/fill loop might get idiom-recognized in.
    const std::string programText = programModule.text() + runtimeModule->text();

    std::string linkerError;
    std::unique_ptr<LldLinker> linker = LldLinker::discover(linkerError);
    if (linker == nullptr)
    {
        error = linkerError;
        return false;
    }

    // Import libraries -- runtime surface plus the project's external `extern C from` imports
    // -- come from the ONE shared table (see WindowsRuntimeImports.h for the history of why a
    // single source of truth replaced the duplicated arrays that used to live right here).
    std::vector<std::filesystem::path> importLibraries;
    if (!createWindowsImportLibraries(
            *linker, temporaryDirectory->path(), programText, externalImports_, importLibraries, error))
    {
        return false;
    }
    for (const std::filesystem::path& extra : extraLinkLibraries_)
    {
        importLibraries.push_back(extra);
    }

    const std::array<std::filesystem::path, 2> objectFiles = {programObject, runtimeObject};
    if (!linker->linkConsoleExecutable(objectFiles, importLibraries, outputPath, linkerError))
    {
        error = linkerError;
        return false;
    }

    PeExecutableVerifier verifier;
    if (!verifier.verify(outputPath))
    {
        std::error_code cleanupError;
        std::filesystem::remove(outputPath, cleanupError);
        error = "linked PE verification failed: " + verifier.error();
        return false;
    }

    info_ = verifier.info();
    return true;
}

void WindowsExecutableBuilder::setExternalImports(std::vector<ExternalImportModule> externalImports)
{
    externalImports_ = std::move(externalImports);
}

void WindowsExecutableBuilder::setExtraLinkLibraries(std::vector<std::filesystem::path> extraLibraries)
{
    extraLinkLibraries_ = std::move(extraLibraries);
}

const PeExecutableInfo& WindowsExecutableBuilder::info() const noexcept
{
    return info_;
}
}
