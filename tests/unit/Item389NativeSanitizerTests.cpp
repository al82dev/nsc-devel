#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/LLVM/LlvmLowerer.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmNativeSanitizer.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/Passes/OptimizationModule.h"
#include "nativesharp/Sema/SemanticAnalyzer.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <memory>
#include <string>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::llvm_backend;

    tests.expect(nativeSanitizerName(NativeSanitizerKind::Address) == "address",
        "address sanitizer has a stable user-facing name");
    tests.expect(nativeSanitizerPipeline(NativeSanitizerKind::Address) == "asan",
        "address sanitizer selects LLVM's native ASan pipeline");
    tests.expect(nativeSanitizerWindowsRuntime(NativeSanitizerKind::Address) ==
            "clang_rt.asan_dynamic-x86_64",
        "Windows ASan runtime identity is explicit and version-independent");

    nativesharp::frontend::FrontendCompilation compilation;
    const bool compiled = compilation.compileBuffer(
        std::filesystem::path("item389_native_sanitizer.ns"),
        "int Read(int* p) { return p[0]; } "
        "int Main() { int* p = stackalloc int[4]; p[1] = 42; return Read(p + 1); }",
        false);
    tests.expect(compiled, "native-memory sanitizer fixture compiles");
    if (!compiled || compilation.semanticAnalyzer() == nullptr ||
        compilation.optimizationModule() == nullptr)
        return tests.result();

    nativesharp::llvm_backend::LlvmLowerer lowerer(
        compilation.semanticAnalyzer()->globalSymbols());
    std::unique_ptr<LlvmModule> module = lowerer.lower(compilation.optimizationModule());
    tests.expect(module != nullptr, "native-memory sanitizer fixture lowers to LLVM");

    std::string error;
    std::unique_ptr<LlvmTargetMachine> target = LlvmTargetMachine::createWindowsX64(
        LlvmOptimizationLevel::O1, error);
    tests.expect(target != nullptr, "Windows x64 sanitizer target is available: " + error);
    if (module == nullptr || target == nullptr) return tests.result();

    LlvmNativeSanitizer sanitizer;
    tests.expect(sanitizer.instrument(*module, *target, NativeSanitizerKind::Address, error),
        "LLVM AddressSanitizer instruments NativeSharp memory operations: " + error);
    const std::string ir = module->text();
    tests.expect(ir.find("__asan_init") != std::string::npos,
        "instrumented module carries the ASan initialization contract");
    tests.expect(ir.find("__asan_report") != std::string::npos,
        "instrumented module carries native memory violation reports");

    std::string verifyError;
    tests.expect(module->verify(verifyError),
        "sanitized LLVM module remains valid: " + verifyError);
    return tests.result();
}
