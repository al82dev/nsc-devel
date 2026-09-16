#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/LLVM/LlvmLowerer.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/Linker/CoffArchiveVerifier.h"
#include "nativesharp/Linker/PeExecutableVerifier.h"
#include "nativesharp/Linker/WindowsExecutableBuilder.h"
#include "nativesharp/Linker/WindowsLibraryBuilder.h"
#include "nativesharp/Linker/WindowsRuntimeLibrary.h"
#include "nativesharp/Passes/OptimizationModule.h"
#include "nativesharp/Runtime/RuntimeAbiContract.h"
#include "nativesharp/Runtime/WindowsRuntimeBuilder.h"
#include "nativesharp/Sema/SemanticAnalyzer.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace
{
std::unique_ptr<nativesharp::llvm_backend::LlvmModule> compileProgram(
    std::string_view source,
    nativesharp::frontend::FrontendCompilation& compilation,
    std::unique_ptr<nativesharp::llvm_backend::LlvmLowerer>& lowerer)
{
    if (!compilation.compileBuffer("item401.ns", std::string(source), true) ||
        compilation.semanticAnalyzer() == nullptr || compilation.optimizationModule() == nullptr)
    {
        return nullptr;
    }
    lowerer = std::make_unique<nativesharp::llvm_backend::LlvmLowerer>(
        compilation.semanticAnalyzer()->globalSymbols());
    return lowerer->lower(compilation.optimizationModule());
}
}

int main()
{
    using namespace nativesharp;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const std::filesystem::path evidence =
        nativesharp::tests::evidenceRoot("nativesharp_item401_independent_windows_runtime");
    std::error_code filesystemError;
    std::filesystem::remove_all(evidence, filesystemError);
    filesystemError.clear();
    std::filesystem::create_directories(evidence, filesystemError);
    tests.expect(!filesystemError, "item 401 evidence directory is available");

    const std::filesystem::path runtimeLibrary =
        evidence / std::string(linker::kWindowsRuntimeLibraryFileName);
    std::string error;
    tests.expect(linker::kWindowsRuntimeLibraryFormatVersion == 1,
        "independent Windows runtime library format is versioned");
    tests.expect(linker::writeWindowsRuntimeLibrary(runtimeLibrary, error) && error.empty(),
        "toolchain emits the independent optimized Win32 runtime archive");
    tests.expect(linker::verifyWindowsRuntimeLibrary(runtimeLibrary, error) && error.empty(),
        "independent Win32 runtime archive verifies its complete ABI surface");

    linker::CoffArchiveVerifier runtimeVerifier;
    tests.expect(runtimeVerifier.verify(runtimeLibrary),
        "independent Win32 runtime is a valid COFF archive");
    if (runtimeVerifier.verify(runtimeLibrary))
    {
        const auto& symbols = runtimeVerifier.info().symbolNames;
        tests.expect(std::find(symbols.begin(), symbols.end(), "__nativesharp_alloc") != symbols.end(),
            "independent runtime archive owns support ABI symbols");
        tests.expect(std::find(symbols.begin(), symbols.end(), "__nativesharp_start") == symbols.end(),
            "independent runtime archive does not capture a program-specific entry point");
    }

    runtime::WindowsRuntimeBuilder runtimeBuilder;
    std::unique_ptr<llvm_backend::LlvmModule> entry = runtimeBuilder.buildEntry("Main", error);
    tests.expect(entry != nullptr && error.empty(),
        "program-specific entry point is emitted as a separate module");
    if (entry != nullptr)
    {
        const std::string entryText = entry->text();
        tests.expect(runtime::verifyRuntimeAbiContract(
                         entryText, runtime::RuntimeAbiImageKind::EntryPoint, error),
            "separate entry module satisfies the entry-only runtime ABI image contract");
        tests.expect(entryText.find("declare i32 @__nativesharp_eh_init()") != std::string::npos &&
                         entryText.find("define i32 @__nativesharp_eh_init()") == std::string::npos,
            "entry module consumes support initialization from the independent archive");
    }

    std::unique_ptr<llvm_backend::LlvmModule> dllAnchor =
        runtimeBuilder.buildDynamicLibraryAnchor(error);
    tests.expect(dllAnchor != nullptr && error.empty(),
        "metadata-only DLLs receive a minimal independent-runtime anchor");
    if (dllAnchor != nullptr)
    {
        tests.expect(dllAnchor->text().find("@__nsc_dll_anchor = constant i8 0") !=
                std::string::npos,
            "DLL anchor contributes a real section without exporting runtime support");
    }

    frontend::FrontendCompilation executableCompilation;
    std::unique_ptr<llvm_backend::LlvmLowerer> executableLowerer;
    std::unique_ptr<llvm_backend::LlvmModule> executableModule =
        compileProgram("int Main() { return 17; }", executableCompilation, executableLowerer);
    tests.expect(executableModule != nullptr, "independent-runtime executable fixture lowers to LLVM");
    const std::filesystem::path executable = evidence / "item401.exe";
    if (executableModule != nullptr)
    {
        linker::WindowsExecutableBuilder executableBuilder;
        executableBuilder.setRuntimeLibrary(runtimeLibrary);
        tests.expect(executableBuilder.build(*executableModule, executable, error) && error.empty(),
            "executable links a tiny entry object with the independent runtime archive");
        linker::PeExecutableVerifier executableVerifier;
        tests.expect(executableVerifier.verify(executable),
            "independent-runtime executable is a valid native PE image");
        const int processResult = std::system(("\"" + executable.string() + "\"").c_str());
        tests.expect(processResult == 17,
            "independent-runtime executable starts and returns through the separate entry object");
    }

    frontend::FrontendCompilation libraryCompilation;
    std::unique_ptr<llvm_backend::LlvmLowerer> libraryLowerer;
    std::unique_ptr<llvm_backend::LlvmModule> libraryModule =
        compileProgram("int Add(int left, int right) { return left + right; } int Main() { return Add(1, 2); }",
            libraryCompilation, libraryLowerer);
    tests.expect(libraryModule != nullptr, "independent-runtime library fixture lowers to LLVM");
    const std::filesystem::path staticLibrary = evidence / "item401-user.lib";
    if (libraryModule != nullptr)
    {
        linker::WindowsLibraryBuilder libraryBuilder;
        libraryBuilder.setRuntimeLibrary(runtimeLibrary);
        tests.expect(libraryBuilder.buildStatic(*libraryModule, staticLibrary,
                         llvm_backend::LlvmOptimizationLevel::O0, error) && error.empty(),
            "user static library no longer embeds a private copy of the runtime");
        linker::CoffArchiveVerifier userVerifier;
        tests.expect(userVerifier.verify(staticLibrary), "user static library remains a valid COFF archive");
        if (userVerifier.verify(staticLibrary))
        {
            const auto& symbols = userVerifier.info().symbolNames;
            tests.expect(std::find(symbols.begin(), symbols.end(), "__nativesharp_alloc") == symbols.end(),
                "user static library defers runtime ownership to the independent archive");
        }
    }

    return tests.resultKeeping(evidence);
}
