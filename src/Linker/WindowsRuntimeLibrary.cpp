#include "nativesharp/Linker/WindowsRuntimeLibrary.h"
#include "Internal/TemporaryFiles.h"

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

namespace
{
bool prepareRuntimeLibraryDestination(
    const std::filesystem::path& destination,
    std::filesystem::path& parent,
    std::string& error)
{
    if (destination.empty())
    {
        error = "Windows runtime library destination is empty";
        return false;
    }
    parent = destination.parent_path();
    if (parent.empty()) return true;
    std::error_code filesystemError;
    std::filesystem::create_directories(parent, filesystemError);
    if (!filesystemError) return true;
    error = "cannot create Windows runtime library directory '" + parent.string() +
        "': " + filesystemError.message();
    return false;
}

bool buildOptimizedWindowsRuntime(
    std::unique_ptr<llvm_backend::LlvmModule>& module,
    std::unique_ptr<llvm_backend::LlvmTargetMachine>& targetMachine,
    std::string& error)
{
    runtime::WindowsRuntimeBuilder builder;
    module = builder.buildSupport(error);
    if (module == nullptr)
    {
        error = "cannot construct independent Windows runtime: " + error;
        return false;
    }
    std::string targetError;
    targetMachine = llvm_backend::LlvmTargetMachine::createWindowsX64(
        llvm_backend::LlvmOptimizationLevel::O2, targetError);
    if (targetMachine == nullptr)
    {
        error = "cannot create Windows runtime target machine: " + targetError;
        return false;
    }
    llvm_backend::LlvmOptimizer optimizer;
    llvm_backend::LlvmOptimizationObservation observation;
    if (optimizer.optimize(*module, *targetMachine, llvm_backend::LlvmOptimizationLevel::O2,
            &observation, targetError))
    {
        return true;
    }
    error = "independent Windows runtime optimization failed: " + targetError;
    return false;
}

bool createRuntimeTemporaryFiles(
    const std::filesystem::path& parent,
    std::unique_ptr<TemporaryFiles>& temporary,
    std::string& error)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path temporaryDirectory =
        parent / (".nsc-runtime-" + std::to_string(stamp));
    std::error_code filesystemError;
    std::filesystem::create_directories(temporaryDirectory, filesystemError);
    if (filesystemError)
    {
        error = "cannot create Windows runtime temporary directory '" +
            temporaryDirectory.string() + "': " + filesystemError.message();
        return false;
    }
    temporary = std::make_unique<TemporaryFiles>(
        temporaryDirectory,
        temporaryDirectory / "NativeSharp.Runtime.Win32.obj",
        temporaryDirectory / std::string(kWindowsRuntimeLibraryFileName));
    return true;
}

bool emitAndVerifyRuntimeObject(
    const llvm_backend::LlvmModule& module,
    const llvm_backend::LlvmTargetMachine& targetMachine,
    const TemporaryFiles& temporary,
    std::string& error)
{
    std::string targetError;
    if (!targetMachine.emit(module, temporary.object(),
            llvm_backend::LlvmCodeGenFileType::Object, targetError))
    {
        error = "independent Windows runtime object emission failed: " + targetError;
        return false;
    }
    llvm_backend::CoffObjectVerifier objectVerifier;
    if (objectVerifier.verify(temporary.object())) return true;
    error = "independent Windows runtime object verification failed: " + objectVerifier.error();
    return false;
}

bool createAndVerifyRuntimeArchive(const TemporaryFiles& temporary, std::string& error)
{
    std::string targetError;
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
    return verifyWindowsRuntimeLibrary(temporary.archive(), error);
}

bool publishRuntimeArchive(
    const TemporaryFiles& temporary,
    const std::filesystem::path& destination,
    std::string& error)
{
    std::error_code filesystemError;
    std::filesystem::remove(destination, filesystemError);
    filesystemError.clear();
    std::filesystem::rename(temporary.archive(), destination, filesystemError);
    if (!filesystemError) return true;
    error = "cannot publish Windows runtime library '" + destination.string() +
        "': " + filesystemError.message();
    return false;
}
}

bool writeWindowsRuntimeLibrary(const std::filesystem::path& destination, std::string& error)
{
    error.clear();
    std::filesystem::path parent;
    if (!prepareRuntimeLibraryDestination(destination, parent, error)) return false;
    std::unique_ptr<llvm_backend::LlvmModule> module;
    std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine;
    if (!buildOptimizedWindowsRuntime(module, targetMachine, error)) return false;
    std::unique_ptr<TemporaryFiles> temporary;
    if (!createRuntimeTemporaryFiles(parent, temporary, error)) return false;
    if (!emitAndVerifyRuntimeObject(*module, *targetMachine, *temporary, error)) return false;
    if (!createAndVerifyRuntimeArchive(*temporary, error)) return false;
    return publishRuntimeArchive(*temporary, destination, error);
}
}
