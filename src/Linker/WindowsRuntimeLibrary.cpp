#include "nativesharp/Linker/WindowsRuntimeLibrary.h"

#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/Linker/CoffArchiveVerifier.h"
#include "nativesharp/Linker/LldLinker.h"
#include "nativesharp/Runtime/RuntimeAbiContract.h"
#include "nativesharp/Runtime/WindowsRuntimeBuilder.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <span>
#include <system_error>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

namespace nativesharp::linker
{
namespace
{
class TemporaryFiles final
{
public:
    TemporaryFiles(
        std::filesystem::path directory,
        std::filesystem::path object,
        std::filesystem::path archive)
        : directory_(std::move(directory)),
          object_(std::move(object)),
          archive_(std::move(archive)) {}
    ~TemporaryFiles()
    {
        std::error_code ignored;
        std::filesystem::remove_all(directory_, ignored);
    }
    const std::filesystem::path& object() const noexcept { return object_; }
    const std::filesystem::path& archive() const noexcept { return archive_; }

private:
    std::filesystem::path directory_;
    std::filesystem::path object_;
    std::filesystem::path archive_;
};

std::filesystem::path executablePath()
{
#ifdef _WIN32
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length != 0 && length < buffer.size())
    {
        buffer.resize(length);
        return std::filesystem::path(buffer);
    }
#endif
    return {};
}
}

std::filesystem::path resolveWindowsRuntimeLibrary(
    const std::filesystem::path& executable,
    const std::filesystem::path& configuredDefault)
{
    if (!executable.empty())
    {
        const std::filesystem::path relative =
            executable.parent_path().parent_path() / "lib" / "nativesharp" / "runtime" /
            std::string(kWindowsRuntimeLibraryFileName);
        std::error_code filesystemError;
        if (std::filesystem::is_regular_file(relative, filesystemError)) return relative;
        if (configuredDefault.empty()) return relative;
    }
    return configuredDefault;
}

std::filesystem::path configuredWindowsRuntimeLibrary()
{
#ifdef _WIN32
    char* overridePath = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&overridePath, &length, "NSC_WINDOWS_RUNTIME_LIBRARY") == 0 &&
        overridePath != nullptr)
    {
        std::filesystem::path result(overridePath);
        std::free(overridePath);
        if (!result.empty()) return result;
    }
#else
    if (const char* overridePath = std::getenv("NSC_WINDOWS_RUNTIME_LIBRARY"))
    {
        if (*overridePath != '\0') return std::filesystem::path(overridePath);
    }
#endif
    return resolveWindowsRuntimeLibrary(
        executablePath(), std::filesystem::path(NSC_DEFAULT_WINDOWS_RUNTIME_LIBRARY));
}

bool verifyWindowsRuntimeLibrary(const std::filesystem::path& source, std::string& error)
{
    error.clear();
    CoffArchiveVerifier verifier;
    if (!verifier.verify(source))
    {
        error = "Windows runtime library verification failed for '" +
            source.generic_string() + "': " + verifier.error();
        return false;
    }
    const std::vector<std::string>& symbols = verifier.info().symbolNames;
    for (const runtime::RuntimeAbiSymbol& expected : runtime::kRuntimeAbiSymbols)
    {
        const bool present = std::find(symbols.begin(), symbols.end(), expected.name) != symbols.end();
        if (expected.name == "__nativesharp_start")
        {
            if (present)
            {
                error = "Windows runtime library must not own executable entry symbol '__nativesharp_start'";
                return false;
            }
            continue;
        }
        if (!present)
        {
            error = "Windows runtime library is missing ABI symbol '" + std::string(expected.name) + "'";
            return false;
        }
    }
    return true;
}

bool writeWindowsRuntimeLibrary(const std::filesystem::path& destination, std::string& error)
{
    error.clear();
    if (destination.empty())
    {
        error = "Windows runtime library destination is empty";
        return false;
    }
    const std::filesystem::path parent = destination.parent_path();
    std::error_code filesystemError;
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent, filesystemError);
        if (filesystemError)
        {
            error = "cannot create Windows runtime library directory '" + parent.string() +
                "': " + filesystemError.message();
            return false;
        }
    }

    runtime::WindowsRuntimeBuilder builder;
    std::unique_ptr<llvm_backend::LlvmModule> module = builder.buildSupport(error);
    if (module == nullptr)
    {
        error = "cannot construct independent Windows runtime: " + error;
        return false;
    }
    std::string targetError;
    std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine =
        llvm_backend::LlvmTargetMachine::createWindowsX64(
            llvm_backend::LlvmOptimizationLevel::O2, targetError);
    if (targetMachine == nullptr)
    {
        error = "cannot create Windows runtime target machine: " + targetError;
        return false;
    }
    llvm_backend::LlvmOptimizer optimizer;
    llvm_backend::LlvmOptimizationObservation observation;
    if (!optimizer.optimize(*module, *targetMachine, llvm_backend::LlvmOptimizationLevel::O2,
            &observation, targetError))
    {
        error = "independent Windows runtime optimization failed: " + targetError;
        return false;
    }

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path temporaryDirectory =
        parent / (".nsc-runtime-" + std::to_string(stamp));
    std::filesystem::create_directories(temporaryDirectory, filesystemError);
    if (filesystemError)
    {
        error = "cannot create Windows runtime temporary directory '" +
            temporaryDirectory.string() + "': " + filesystemError.message();
        return false;
    }
    TemporaryFiles temporary(
        temporaryDirectory,
        temporaryDirectory / "NativeSharp.Runtime.Win32.obj",
        temporaryDirectory / std::string(kWindowsRuntimeLibraryFileName));
    if (!targetMachine->emit(*module, temporary.object(),
            llvm_backend::LlvmCodeGenFileType::Object, targetError))
    {
        error = "independent Windows runtime object emission failed: " + targetError;
        return false;
    }
    llvm_backend::CoffObjectVerifier objectVerifier;
    if (!objectVerifier.verify(temporary.object()))
    {
        error = "independent Windows runtime object verification failed: " + objectVerifier.error();
        return false;
    }

    std::unique_ptr<LldLinker> linker = LldLinker::discover(targetError);
    if (linker == nullptr)
    {
        error = targetError;
        return false;
    }
    const std::vector<std::filesystem::path> objects = {temporary.object()};
    if (!linker->createStaticLibrary(objects, temporary.archive(), targetError))
    {
        error = "independent Windows runtime archive creation failed: " + targetError;
        return false;
    }
    if (!verifyWindowsRuntimeLibrary(temporary.archive(), error))
    {
        return false;
    }

    filesystemError.clear();
    std::filesystem::remove(destination, filesystemError);
    filesystemError.clear();
    std::filesystem::rename(temporary.archive(), destination, filesystemError);
    if (filesystemError)
    {
        error = "cannot publish Windows runtime library '" + destination.string() +
            "': " + filesystemError.message();
        return false;
    }
    return true;
}
}
