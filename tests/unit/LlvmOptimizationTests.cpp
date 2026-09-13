#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/LLVM/LlvmLowerer.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmOptimizationObservation.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmProcessTrace.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/Passes/OptimizationModule.h"
#include "nativesharp/Sema/SemanticAnalyzer.h"

#include "../support/TestContext.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace
{
using nativesharp::frontend::FrontendCompilation;
using nativesharp::llvm_backend::LlvmLowerer;
using nativesharp::llvm_backend::LlvmModule;
using nativesharp::llvm_backend::LlvmOptimizationLevel;
using nativesharp::llvm_backend::kLlvmRepeatedPassSuspicion;
using nativesharp::llvm_backend::kLlvmProcessTraceBegin;
using nativesharp::llvm_backend::kLlvmProcessTraceEnd;
using nativesharp::llvm_backend::kLlvmStandardErrorProbe;
using nativesharp::llvm_backend::LlvmOptimizationObservation;
using nativesharp::llvm_backend::LlvmOptimizationBudgetScope;
using nativesharp::llvm_backend::LlvmOptimizationBudgetTracker;
using nativesharp::llvm_backend::LlvmOptimizer;
using nativesharp::llvm_backend::observeActiveLlvmOptimizationBudget;
using nativesharp::llvm_backend::llvmProcessTraceRequired;
using nativesharp::llvm_backend::llvmVerifyEachPassRequired;
using nativesharp::llvm_backend::observeLlvmPassLog;
using nativesharp::llvm_backend::LlvmTargetMachine;

std::unique_ptr<LlvmModule> compileAndLower(
    std::string_view source,
    FrontendCompilation& compilation,
    std::unique_ptr<LlvmLowerer>& lowerer)
{
    if (!compilation.compileBuffer(
            std::filesystem::path("llvm_optimization_test.ns"),
            std::string(source),
            false))
    {
        return nullptr;
    }
    if (compilation.semanticAnalyzer() == nullptr || compilation.optimizationModule() == nullptr)
    {
        return nullptr;
    }

    lowerer = std::make_unique<LlvmLowerer>(compilation.semanticAnalyzer()->globalSymbols());
    return lowerer->lower(compilation.optimizationModule());
}

std::unique_ptr<LlvmModule> makeInliningFixture(
    FrontendCompilation& compilation,
    std::unique_ptr<LlvmLowerer>& lowerer)
{
    return compileAndLower(
        "int Square(int x) { return x * x; } "
        "int Main() { return Square(6) + Square(2); }",
        compilation,
        lowerer);
}

#if defined(_WIN32)
constexpr std::string_view kProcessTraceTestChild = "--nsc-test-llvm-trace-child";

// Este codigo corre en un proceso cuya stderr ya apuntaba al fichero del padre ANTES de cargar
// LLVM-C.dll. Ademas de producir un registro real, compara el IR con y sin debug logging para que
// trasladar la captura fuera del proceso no debilite el invariante de equivalencia.
int runProcessTraceTestChild()
{
    nativesharp::llvm_backend::setLlvmProcessTraceActive(true);
    LlvmOptimizer optimizer;
    for (const LlvmOptimizationLevel level : {LlvmOptimizationLevel::O0,
                                              LlvmOptimizationLevel::O1,
                                              LlvmOptimizationLevel::O2})
    {
        std::string plainText;
        std::string observedText;
        for (const bool observed : {false, true})
        {
            std::string targetError;
            std::unique_ptr<LlvmTargetMachine> targetMachine =
                LlvmTargetMachine::createWindowsX64(level, targetError);
            FrontendCompilation compilation;
            std::unique_ptr<LlvmLowerer> lowerer;
            std::unique_ptr<LlvmModule> module = makeInliningFixture(compilation, lowerer);
            if (targetMachine == nullptr || module == nullptr) return 2;

            LlvmOptimizationObservation observation;
            std::string optimizationError;
            if (!optimizer.optimize(
                    *module, *targetMachine, level, observed ? &observation : nullptr,
                    optimizationError))
            {
                return 3;
            }
            (observed ? observedText : plainText) = module->text();
        }
        if (plainText.empty() || plainText != observedText) return 4;
    }
    return 0;
}
#endif
}

int main(int argc, char** argv)
{
#if defined(_WIN32)
    if (argc == 2 && std::string_view(argv[1]) == kProcessTraceTestChild)
    {
        return runProcessTraceTestChild();
    }
#endif

    using nativesharp::llvm_backend::llvmOptimizationLevelName;
    using nativesharp::llvm_backend::llvmOptimizationPipelineName;
    using nativesharp::tests::TestContext;

    TestContext tests;

    tests.expect(llvmOptimizationLevelName(LlvmOptimizationLevel::O0) == "O0", "O0 has a stable display name");
    tests.expect(llvmOptimizationLevelName(LlvmOptimizationLevel::O1) == "O1", "O1 has a stable display name");
    tests.expect(llvmOptimizationLevelName(LlvmOptimizationLevel::O2) == "O2", "O2 has a stable display name");
    tests.expect(llvmOptimizationLevelName(LlvmOptimizationLevel::O3) == "O3", "O3 has a stable display name");
    tests.expect(llvmOptimizationPipelineName(LlvmOptimizationLevel::O0) == "default<O0>",
        "O0 selects the LLVM default O0 pipeline");
    tests.expect(llvmOptimizationPipelineName(LlvmOptimizationLevel::O1) == "default<O1>",
        "O1 selects the LLVM default O1 pipeline");
    tests.expect(llvmOptimizationPipelineName(LlvmOptimizationLevel::O2) == "default<O2>",
        "O2 selects the LLVM default O2 pipeline");
    tests.expect(llvmOptimizationPipelineName(LlvmOptimizationLevel::O3) == "default<O3>",
        "O3 selects the LLVM default O3 pipeline");

    tests.expect(!llvmProcessTraceRequired(std::array<std::string_view, 1>{"--version"}),
        "non-compilation commands do not launch the LLVM trace supervisor");
    tests.expect(!llvmProcessTraceRequired(
            std::array<std::string_view, 2>{"--emit-project-oop-obj", "-O0"}),
        "explicit O0 compilation does not launch the LLVM trace supervisor");
    tests.expect(!llvmProcessTraceRequired(
            std::array<std::string_view, 1>{"--emit-project-oop-obj"}),
        "the default O0 compilation does not launch the LLVM trace supervisor");
    tests.expect(!llvmProcessTraceRequired(
            std::array<std::string_view, 2>{"--emit-project-oop-obj", "-O2"}),
        "an ordinary O2 compilation does not pay for the full LLVM pass trace");
    tests.expect(llvmProcessTraceRequired(
            std::array<std::string_view, 3>{"--emit-project-oop-obj", "-O1", "--llvm-pass-trace"}) &&
            llvmProcessTraceRequired(
                std::array<std::string_view, 3>{"--emit-project-oop-obj", "-O2", "--llvm-pass-trace"}) &&
            llvmProcessTraceRequired(
                std::array<std::string_view, 3>{"--emit-project-oop-obj", "-O3", "--llvm-pass-trace"}),
        "optimized levels retain full tracing when explicitly requested");
    tests.expect(!llvmVerifyEachPassRequired(LlvmOptimizationLevel::O0, true),
        "O0 relies on optimizer boundary verification instead of verifying after every pass");
    tests.expect(!llvmVerifyEachPassRequired(LlvmOptimizationLevel::O2, false),
        "ordinary O2 relies on optimizer boundary verification");
    tests.expect(llvmVerifyEachPassRequired(LlvmOptimizationLevel::O1, true) &&
            llvmVerifyEachPassRequired(LlvmOptimizationLevel::O2, true) &&
            llvmVerifyEachPassRequired(LlvmOptimizationLevel::O3, true),
        "explicit detailed diagnostics verify every optimized pass");

    {
        LlvmOptimizationBudgetTracker budget(10);
        budget.observe(LlvmOptimizationLevel::O0, std::chrono::milliseconds(20));
        tests.expect(budget.invocations() == 0 && budget.elapsed().count() == 0,
            "the O2 budget ignores work from other optimization levels");
        budget.observe(LlvmOptimizationLevel::O2, std::chrono::milliseconds(4));
        budget.observe(LlvmOptimizationLevel::O2, std::chrono::milliseconds(7));
        tests.expect(budget.invocations() == 2 && budget.exceeded(),
            "the O2 budget accumulates every optimizer invocation in the compilation");
        tests.expect(budget.report() ==
                "optimization-budget level=O2 budget_ms=10 elapsed_us=11000 invocations=2 status=exceeded",
            "the O2 budget has a stable machine-readable report");
    }

    {
        LlvmOptimizationBudgetTracker outer(100);
        LlvmOptimizationBudgetTracker inner(100);
        {
            LlvmOptimizationBudgetScope outerScope(outer);
            observeActiveLlvmOptimizationBudget(
                LlvmOptimizationLevel::O2, std::chrono::milliseconds(3));
            {
                LlvmOptimizationBudgetScope innerScope(inner);
                observeActiveLlvmOptimizationBudget(
                    LlvmOptimizationLevel::O2, std::chrono::milliseconds(5));
            }
            observeActiveLlvmOptimizationBudget(
                LlvmOptimizationLevel::O2, std::chrono::milliseconds(7));
        }
        tests.expect(outer.elapsed() == std::chrono::milliseconds(10) &&
                inner.elapsed() == std::chrono::milliseconds(5),
            "nested budget scopes restore the previous compiler-thread tracker");
    }

    constexpr std::array<LlvmOptimizationLevel, 4> levels = {
        LlvmOptimizationLevel::O0,
        LlvmOptimizationLevel::O1,
        LlvmOptimizationLevel::O2,
        LlvmOptimizationLevel::O3};

    LlvmOptimizer optimizer;
    for (const LlvmOptimizationLevel level : levels)
    {
        std::string targetError;
        std::unique_ptr<LlvmTargetMachine> targetMachine = LlvmTargetMachine::createWindowsX64(level, targetError);
        tests.expect(targetMachine != nullptr, "each supported level creates a Windows x64 TargetMachine");
        if (targetMachine == nullptr)
        {
            continue;
        }
        tests.expect(targetMachine->optimizationLevel() == level,
            "TargetMachine retains the requested optimization level");

        FrontendCompilation compilation;
        std::unique_ptr<LlvmLowerer> lowerer;
        std::unique_ptr<LlvmModule> module = makeInliningFixture(compilation, lowerer);
        tests.expect(module != nullptr, "optimization fixture lowers for every supported level");
        if (module == nullptr)
        {
            continue;
        }

        std::string optimizationError;
        tests.expect(optimizer.optimize(*module, *targetMachine, level, optimizationError),
            "LLVM New Pass Manager accepts every supported default pipeline");
        tests.expect(optimizationError.empty(), "successful LLVM optimization keeps its diagnostic empty");

        std::string verificationError;
        tests.expect(module->verify(verificationError), "optimized module passes LLVM verification");
    }

    {
        FrontendCompilation compilation;
        std::unique_ptr<LlvmLowerer> lowerer;
        std::unique_ptr<LlvmModule> module = makeInliningFixture(compilation, lowerer);
        tests.expect(module != nullptr, "raw comparison fixture lowers successfully");
        if (module != nullptr)
        {
            const std::string raw = module->text();
            tests.expect(raw.find("call i32 @Square(i32 6)") != std::string::npos &&
                    raw.find("call i32 @Square(i32 2)") != std::string::npos,
                "raw LLVM boundary still exposes both calls before LLVM optimization");
        }
    }

    {
        std::string targetError;
        std::unique_ptr<LlvmTargetMachine> targetMachine =
            LlvmTargetMachine::createWindowsX64(LlvmOptimizationLevel::O0, targetError);
        FrontendCompilation compilation;
        std::unique_ptr<LlvmLowerer> lowerer;
        std::unique_ptr<LlvmModule> module = makeInliningFixture(compilation, lowerer);
        if (targetMachine != nullptr && module != nullptr)
        {
            std::string optimizationError;
            tests.expect(optimizer.optimize(
                    *module, *targetMachine, LlvmOptimizationLevel::O0, optimizationError),
                "O0 optimization succeeds for the comparison fixture");
            const std::string text = module->text();
            tests.expect(text.find("call i32 @Square(i32 6)") != std::string::npos,
                "O0 keeps the non-forced Square call visible");
        }
    }

    {
        std::string targetError;
        std::unique_ptr<LlvmTargetMachine> targetMachine =
            LlvmTargetMachine::createWindowsX64(LlvmOptimizationLevel::O2, targetError);
        FrontendCompilation compilation;
        std::unique_ptr<LlvmLowerer> lowerer;
        std::unique_ptr<LlvmModule> module = makeInliningFixture(compilation, lowerer);
        if (targetMachine != nullptr && module != nullptr)
        {
            std::string optimizationError;
            tests.expect(optimizer.optimize(
                    *module, *targetMachine, LlvmOptimizationLevel::O2, optimizationError),
                "O2 optimization succeeds for the comparison fixture");
            const std::string text = module->text();
            tests.expect(text.find("call i32 @Square") == std::string::npos,
                "O2 removes the constant call sites from Main");
            tests.expect(text.find("ret i32 40") != std::string::npos,
                "O2 folds the inlined expression to the expected constant return");
        }
    }

    {
        std::string targetError;
        std::unique_ptr<LlvmTargetMachine> targetMachine =
            LlvmTargetMachine::createWindowsX64(LlvmOptimizationLevel::O2, targetError);
        FrontendCompilation compilation;
        std::unique_ptr<LlvmLowerer> lowerer;
        std::unique_ptr<LlvmModule> module = compileAndLower(
            "int Divide(int a, int b) { return a / b; }",
            compilation,
            lowerer);
        if (targetMachine != nullptr && module != nullptr)
        {
            std::string optimizationError;
            tests.expect(optimizer.optimize(
                    *module, *targetMachine, LlvmOptimizationLevel::O2, optimizationError),
                "O2 accepts a function containing NativeSharp dynamic trap guards");
            const std::string text = module->text();
            tests.expect(text.find("__nativesharp_trap") != std::string::npos,
                "O2 preserves a trap path when the divide precondition is dynamic");
            tests.expect(text.find("sdiv i32") != std::string::npos,
                "O2 preserves the actual signed division after its guard");
        }
    }

    {
        constexpr std::array<LlvmOptimizationLevel, 2> aggressiveLevels = {
            LlvmOptimizationLevel::O2,
            LlvmOptimizationLevel::O3};
        for (const LlvmOptimizationLevel level : aggressiveLevels)
        {
            std::string targetError;
            std::unique_ptr<LlvmTargetMachine> targetMachine =
                LlvmTargetMachine::createWindowsX64(level, targetError);
            FrontendCompilation compilation;
            std::unique_ptr<LlvmLowerer> lowerer;
            std::unique_ptr<LlvmModule> module = compileAndLower(
                "int Guarded(int value, int shift, double floating) "
                "{ return (value / shift) + (value << shift) + (int)floating; }",
                compilation,
                lowerer);
            tests.expect(module != nullptr, "semantic-guard fixture lowers successfully");
            if (targetMachine == nullptr || module == nullptr)
            {
                continue;
            }

            std::string optimizationError;
            tests.expect(optimizer.optimize(*module, *targetMachine, level, optimizationError),
                "aggressive LLVM optimization accepts multiple NativeSharp trap guards");
            const std::string text = module->text();
            tests.expect(text.find("__nativesharp_trap(i32 1)") != std::string::npos,
                "aggressive LLVM optimization preserves the division-by-zero trap");
            // LA RAZON 2 YA NO SE EMITE, asi que esta comprobacion pasa de exigirla a exigir que
            // NO este. `MinValue / -1` dejo de ser un fallo del programa: envuelve, por el doc 110
            // §2.1 y §2.5, y el bajador emite la correccion sin rama en vez de la trampa. El doc
            // 113 tiene la medicion completa.
            //
            // LO QUE ESTA COMPROBACION PROTEGIA --que el optimizador agresivo no borre lo que el
            // compilador emitio-- lo siguen protegiendo las razones 1, 3 y 5 de este mismo bucle,
            // que no han cambiado.
            //
            // Y QUE LA CORRECCION SOBREVIVA A -O2/-O3 NO SE COMPRUEBA AQUI a proposito. Se podria
            // buscar el `select` en el texto optimizado, pero eso fija una FORMA del IR que
            // InstCombine puede reescribir legitimamente sin cambiar el resultado, y una prueba
            // asi se pone roja por un cambio de LLVM que no rompe nada. La propiedad de verdad
            // --que el resultado sea MinValue despues de optimizar-- la mide la fixture
            // tests/cc1/c30_contratos EJECUTANDO en -O2, que es donde el fallo original aparecio.
            tests.expect(text.find("__nativesharp_trap(i32 2)") == std::string::npos,
                "signed-division overflow is no longer a trap after aggressive optimization");
            tests.expect(text.find("__nativesharp_trap(i32 3)") != std::string::npos,
                "aggressive LLVM optimization preserves the invalid-shift-count trap");
            tests.expect(text.find("__nativesharp_trap(i32 5)") != std::string::npos,
                "aggressive LLVM optimization preserves the floating-to-integer-range trap");
            tests.expect(text.find("sdiv i32") != std::string::npos,
                "aggressive LLVM optimization retains the guarded signed division operation");
            tests.expect(text.find("shl i32") != std::string::npos,
                "aggressive LLVM optimization retains the guarded shift operation");
            tests.expect(text.find("fptosi") != std::string::npos,
                "aggressive LLVM optimization retains the guarded floating-to-integer conversion");
        }
    }

    {
        std::string targetError;
        std::unique_ptr<LlvmTargetMachine> targetMachine =
            LlvmTargetMachine::createWindowsX64(LlvmOptimizationLevel::O2, targetError);
        FrontendCompilation compilation;
        std::unique_ptr<LlvmLowerer> lowerer;
        std::unique_ptr<LlvmModule> module = compileAndLower(
            "int Add(int left, int right) { return left + right; }",
            compilation,
            lowerer);
        tests.expect(module != nullptr, "signed-wrap fixture lowers successfully");
        if (targetMachine != nullptr && module != nullptr)
        {
            std::string optimizationError;
            tests.expect(optimizer.optimize(
                    *module, *targetMachine, LlvmOptimizationLevel::O2, optimizationError),
                "O2 accepts NativeSharp signed wrapping arithmetic");
            const std::string text = module->text();
            tests.expect(text.find("add i32") != std::string::npos,
                "O2 retains the signed wrapping add operation");
            tests.expect(text.find("add nsw i32") == std::string::npos,
                "O2 does not invent a global no-signed-wrap contract for dynamic signed addition");
        }
    }

    {
        std::string targetError;
        std::unique_ptr<LlvmTargetMachine> targetMachine =
            LlvmTargetMachine::createWindowsX64(LlvmOptimizationLevel::O0, targetError);
        FrontendCompilation compilation;
        std::unique_ptr<LlvmLowerer> lowerer;
        std::unique_ptr<LlvmModule> module = makeInliningFixture(compilation, lowerer);
        if (targetMachine != nullptr && module != nullptr)
        {
            std::string optimizationError;
            tests.expect(!optimizer.optimize(
                    *module, *targetMachine, LlvmOptimizationLevel::O2, optimizationError),
                "optimizer rejects a pass level that disagrees with its TargetMachine");
            tests.expect(optimizationError.find("does not match") != std::string::npos,
                "optimization-level mismatch reports a precise diagnostic");
            std::string verificationError;
            tests.expect(module->verify(verificationError),
                "level-mismatch rejection leaves the LLVM module valid");
        }
    }

    {
        std::string targetError;
        std::unique_ptr<LlvmTargetMachine> targetMachine =
            LlvmTargetMachine::createWindowsX64(LlvmOptimizationLevel::O0, targetError);
        FrontendCompilation compilation;
        std::unique_ptr<LlvmLowerer> lowerer;
        std::unique_ptr<LlvmModule> module = makeInliningFixture(compilation, lowerer);
        if (targetMachine != nullptr && module != nullptr)
        {
            std::string optimizationError;
            const auto invalid = static_cast<LlvmOptimizationLevel>(99);
            tests.expect(!optimizer.optimize(*module, *targetMachine, invalid, optimizationError),
                "optimizer rejects an unsupported optimization-level enum value");
            tests.expect(optimizationError.find("unsupported") != std::string::npos,
                "unsupported optimization level reports a precise diagnostic");
            std::string verificationError;
            tests.expect(module->verify(verificationError),
                "rejected optimization request leaves the input module valid");
        }
    }

    // =====================================================================================
    // NSC-LIMIT-005 (Gate 1) -- LO QUE SE OBSERVA DE UNA CORRIDA DEL OPTIMIZADOR.
    // =====================================================================================
    //
    // `LLVMRunPasses` es una sola llamada opaca. Cuando `c38_enum_ancho` devolvia 42 en O0 y 39 en
    // O2, no habia forma de decir con que LLVM, con que pipeline ni por donde iba. Estas
    // comprobaciones fijan lo que el compilador PUEDE decir hoy sin salir de la API C.
    {
        std::string targetError;
        std::unique_ptr<LlvmTargetMachine> targetMachine =
            LlvmTargetMachine::createWindowsX64(LlvmOptimizationLevel::O2, targetError);
        FrontendCompilation compilation;
        std::unique_ptr<LlvmLowerer> lowerer;
        std::unique_ptr<LlvmModule> module = makeInliningFixture(compilation, lowerer);
        tests.expect(module != nullptr && targetMachine != nullptr,
            "observation fixture lowers and gets a TargetMachine");
        if (module != nullptr && targetMachine != nullptr)
        {
            LlvmOptimizationObservation observation;
            std::string optimizationError;
            tests.expect(
                optimizer.optimize(
                    *module, *targetMachine, LlvmOptimizationLevel::O2, &observation,
                    optimizationError),
                "the observed overload optimizes exactly like the plain one");

            // 1. LLVM, pipeline y nivel: los tres datos sin los cuales dos corridas que difieren
            //    no se pueden ni comparar.
            tests.expect(observation.llvmMajor != 0,
                "the observation records the LLVM version actually loaded");
            tests.expect(observation.pipeline == "default<O2>",
                "the observation records the exact pipeline text");
            tests.expect(observation.level == LlvmOptimizationLevel::O2,
                "the observation records the TargetMachine optimization level");

            // 2. Triple y data layout DEL MODULO, leidos tras configureModuleForTarget. El IR de
            //    `--dump-project-oop-llvm` no es el que ve la API C, y reproducir la llamada en
            //    `opt.exe` con otro triple no reproduce nada.
            tests.expect(observation.moduleTriple.find("windows-msvc") != std::string::npos,
                "the observation records the module triple, not the requested one");
            tests.expect(!observation.moduleDataLayout.empty(),
                "the observation records the module data layout");

            // 3. Verificacion a los dos lados. Un modulo que entra invalido y uno que sale
            //    invalido son fallos distintos y hay que poder distinguirlos.
            tests.expect(observation.verifiedBefore && observation.verifiedAfter,
                "the observation records the module verifying before AND after");

#if defined(_WIN32)
            // `LLVM-C.dll` lleva su propio CRT. La captura valida se establece al crear un hijo,
            // no mediante `_dup2` despues de cargar la DLL. El resultado se combina con los datos
            // de modulo que acaba de medir la llamada directa.
            nativesharp::llvm_backend::LlvmTraceChildResult child;
            const bool childStarted =
                nativesharp::llvm_backend::runCurrentExecutableWithCapturedStandardError(
                    kProcessTraceTestChild, child);
            tests.expect(childStarted,
                "the Windows process-boundary pass-log probe starts");
            if (childStarted)
            {
                const nativesharp::llvm_backend::LlvmProcessTraceLog trace =
                    nativesharp::llvm_backend::splitLlvmProcessTrace(child.standardError);
                tests.expect(child.exitCode == 0,
                    "the process-boundary trace preserves byte-identical O2 IR");
                tests.expect(trace.diagnostics.empty(),
                    "the process trace does not misclassify LLVM logging as a diagnostic");
                observation.captureVerified = trace.framingObserved && trace.framingComplete;
                observation.llvmStandardErrorCaptured = trace.llvmStandardErrorCaptured;
                observation.capturedBytes = trace.capturedBytes;
                observeLlvmPassLog(trace.passLog, observation);
            }
#endif

            // 4. El ultimo pass y la ultima unidad OBSERVADOS. Sin esto el reporte de un fallo no
            //    puede cumplir el cierre del Gate 1.
            //
            //    SI ESTO CAE, EL REPORTE DICE POR QUE. La observacion sabe distinguir «no se pudo
            //    desviar la salida de error» de «se desvio y LLVM no escribio nada», y son dos
            //    problemas opuestos: el primero es nuestro y el segundo es de la build de LLVM
            //    contra la que se enlaza. Imprimirlo aqui evita tener que reproducir el fallo solo
            //    para saber cual de los dos es.
            if (observation.observedPassCount == 0)
            {
                std::printf(
                    "DIAGNOSTICO: desvio propio=%s, salida de error de LLVM capturada=%s, "
                    "bytes=%zu\n  %s\n",
                    observation.captureVerified ? "SI" : "NO",
                    observation.llvmStandardErrorCaptured ? "SI" : "NO",
                    observation.capturedBytes,
                    observation.report().c_str());
                std::fflush(stdout);
            }
            tests.expect(observation.observedPassCount > 0,
                "the debug log yields at least one observed pass at O2");
            tests.expect(!observation.lastObservedPass.empty(),
                "the observation names the last observed pass");

            // 4b. EL DESVIO DE LA SALIDA DE ERROR FUNCIONA EN ESTA PLATAFORMA. Se comprueba con
            //     una marca propia escrita dentro de la ventana de captura, asi que no depende de
            //     que LLVM diga nada. Esto SI es responsabilidad del compilador: si cae, la
            //     captura esta rota aqui, no en LLVM.
            tests.expect(observation.captureVerified,
                "the standard-error redirection works on this platform");
            // Y LA DE LLVM LLEGA A ELLA, que es otra pregunta: la marca propia solo demuestra que
            // NUESTRO descriptor esta desviado. En Windows la biblioteca puede venir como DLL con
            // su propio CRT. Sin esto, un registro vacio no distingue «LLVM no escribio» de «LLVM
            // escribio en otro sitio», y son arreglos opuestos.
            tests.expect(observation.llvmStandardErrorCaptured,
                "and LLVM's own standard error goes through that same capture");

            // 5. Y el reporte NO llama culpable a nadie: un log prueba orden de ejecucion, no
            //    causalidad. Si esta cadena aparece, alguien afirmo de mas.
            const std::string report = observation.report();
            tests.expect(report.find("ultimo pass observado") != std::string::npos,
                "the report says 'last observed pass'");
            tests.expect(report.find("culpable") == std::string::npos,
                "and never calls it a culprit: a log proves order, not causality");
            tests.expect(report.find("default<O2>") != std::string::npos &&
                    report.find("windows-msvc") != std::string::npos,
                "the report carries LLVM, pipeline and module identity in one line");
        }
    }

    // LA DETECCION DE FALTA DE CONVERGENCIA, DISPARADA DE VERDAD.
    //
    // Provocar en LLVM una falta de convergencia REAL es caro y fragil: haria falta un modulo
    // patologico que ademas siguiera siendolo en la siguiente version de LLVM, y una deteccion que
    // nadie ha visto dispararse no es una deteccion. Por eso el lector del registro vive fuera del
    // optimizador --`observeLlvmPassLog`-- y se prueba con registros escritos a mano.
    {
        const auto line = [](std::string_view pass, std::string_view unit) {
            std::string text = "Running pass: ";
            text.append(pass);
            if (!unit.empty())
            {
                text += " on ";
                text.append(unit);
            }
            text += '\n';
            return text;
        };
        const auto repeat = [&](std::string_view pass, std::string_view unit, std::size_t times) {
            std::string log;
            for (std::size_t index = 0; index < times; ++index) log += line(pass, unit);
            return log;
        };

        // 1. EL UMBRAL, POR LOS DOS LADOS. `kLlvmRepeatedPassSuspicion` repeticiones exactas NO
        //    son sospechosas --la comparacion es estricta-- y una mas si lo es. Sin el caso de
        //    igualdad, un `>=` colado por accidente pasaria desapercibido.
        {
            LlvmOptimizationObservation observation;
            observeLlvmPassLog(repeat("LoopRotatePass", "bucle", kLlvmRepeatedPassSuspicion),
                observation);
            tests.expect(!observation.convergenceSuspected,
                "exactly the suspicion threshold is not yet suspicious");
            tests.expect(observation.maxRepeatedPass == kLlvmRepeatedPassSuspicion,
                "and the repetition count is the one that was fed in");
        }
        {
            LlvmOptimizationObservation observation;
            observeLlvmPassLog(repeat("LoopRotatePass", "bucle", kLlvmRepeatedPassSuspicion + 1),
                observation);
            tests.expect(observation.convergenceSuspected,
                "one repetition past the threshold raises the suspicion");
            tests.expect(observation.maxRepeatedPass == kLlvmRepeatedPassSuspicion + 1,
                "and the report can say how many times it was");
            tests.expect(observation.lastObservedPass == "LoopRotatePass" &&
                    observation.lastObservedUnit == "bucle",
                "the repeated pass and unit are the ones named");
            const std::string report = observation.report();
            tests.expect(report.find("SE SOSPECHA FALTA DE CONVERGENCIA") != std::string::npos,
                "and the suspicion reaches the text the user reads");
        }

        // 2. EL MISMO PASE SOBRE UNIDADES DISTINTAS NO ES UNA REPETICION, y este es el caso que
        //    decide si la deteccion sirve para algo. LLVM corre cada pase de funcion sobre TODAS
        //    las funciones del modulo: si la clave fuera solo el nombre del pase, cualquier
        //    compilacion normal de un modulo con mas de 64 funciones se marcaria como sospechosa y
        //    el aviso no significaria nada.
        {
            LlvmOptimizationObservation observation;
            std::string log;
            for (std::size_t index = 0; index < kLlvmRepeatedPassSuspicion * 3; ++index)
            {
                log += line("InstCombinePass", "funcion_" + std::to_string(index));
            }
            observeLlvmPassLog(log, observation);
            tests.expect(!observation.convergenceSuspected && observation.maxRepeatedPass == 1,
                "the same pass over different units is not a repetition");
            tests.expect(observation.observedPassCount == kLlvmRepeatedPassSuspicion * 3,
                "though every one of them is counted as observed");
        }

        // 3. «ULTIMO» ES LITERALMENTE EL ULTIMO, no el mas frecuente. La distincion es la misma que
        //    separa observar de acusar: lo que el registro sostiene es donde iba, no quien tuvo la
        //    culpa.
        {
            LlvmOptimizationObservation observation;
            std::string log = repeat("LoopRotatePass", "bucle", kLlvmRepeatedPassSuspicion + 1);
            log += line("AnnotationRemarksPass", "modulo");
            observeLlvmPassLog(log, observation);
            tests.expect(observation.lastObservedPass == "AnnotationRemarksPass" &&
                    observation.lastObservedUnit == "modulo",
                "the last observed pass is the last one, not the most repeated one");
            tests.expect(observation.convergenceSuspected &&
                    observation.maxRepeatedPass == kLlvmRepeatedPassSuspicion + 1,
                "and the repetition is still reported alongside it");
        }

        // 4. UN REGISTRO QUE NO SE PUDO CAPTURAR se dice, no se deja en blanco. Una linea vacia
        //    donde deberia ir un pase se lee como «no hubo pases», que es una afirmacion distinta
        //    y falsa.
        {
            LlvmOptimizationObservation observation;
            observeLlvmPassLog("", observation);
            tests.expect(observation.observedPassCount == 0 && observation.lastObservedPass.empty(),
                "an empty log observes nothing");
            tests.expect(
                observation.report().find("no se observo ningun pass") != std::string::npos,
                "and the report says so instead of leaving a blank where a pass should be");
        }
        {
            LlvmOptimizationObservation observation;
            observeLlvmPassLog("warning: something else entirely\nlld-link: done\n", observation);
            tests.expect(observation.observedPassCount == 0,
                "output that is not a pass log observes nothing either");
        }

        // 5. LAS DOS FORMAS QUE TIENE LA LINEA DE VENIR. Sin unidad --hay pases de modulo que se
        //    imprimen sin el `on`-- y con el `\r` de un fichero abierto en modo texto en Windows,
        //    que es exactamente como llega la captura del descriptor 2.
        {
            LlvmOptimizationObservation observation;
            observeLlvmPassLog("Running pass: VerifierPass\n", observation);
            tests.expect(observation.lastObservedPass == "VerifierPass" &&
                    observation.lastObservedUnit.empty(),
                "a pass logged without a unit keeps its name and leaves the unit empty");
        }
        {
            LlvmOptimizationObservation observation;
            observeLlvmPassLog("Running pass: SROAPass on main\r\n", observation);
            tests.expect(observation.lastObservedPass == "SROAPass" &&
                    observation.lastObservedUnit == "main",
                "a CRLF line does not leave a stray carriage return in the unit name");
        }

        // 6. LOS DELIMITADORES DE PROCESO separan el ruido de LLVM de los diagnosticos de nsc y
        //    aceptan CRLF. `LLVMDumpType` no agrega salto de linea, de modo que la sonda y el
        //    delimitador final llegan pegados en la distribucion real de LLVM 23 para Windows.
        {
            std::string captured = "diagnostico anterior\n";
            captured += kLlvmProcessTraceBegin;
            captured += "\r\nRunning pass: InstCombinePass on Main\r\n%";
            captured += kLlvmStandardErrorProbe;
            captured += " = type opaque";
            captured += kLlvmProcessTraceEnd;
            captured += "\r\ndiagnostico posterior\n";
            const nativesharp::llvm_backend::LlvmProcessTraceLog trace =
                nativesharp::llvm_backend::splitLlvmProcessTrace(captured);
            LlvmOptimizationObservation observation;
            observeLlvmPassLog(trace.passLog, observation);
            tests.expect(trace.framingObserved && trace.framingComplete &&
                    trace.llvmStandardErrorCaptured,
                "process trace framing and the LLVM-owned probe are recognized");
            tests.expect(observation.lastObservedPass == "InstCombinePass" &&
                    observation.lastObservedUnit == "Main",
                "process trace framing preserves the pass payload");
            tests.expect(trace.diagnostics ==
                    "diagnostico anterior\ndiagnostico posterior\n",
                "process trace framing preserves only diagnostics outside LLVM windows");
        }
    }

    // OBSERVAR NO PUEDE CAMBIAR EL RESULTADO, y esto no se argumenta: se mide.
    //
    // En POSIX la observacion enciende `LLVMPassBuilderOptionsSetDebugLogging` y desvia el
    // descriptor 2 mientras dura la llamada. En Windows la misma comparacion O0/O1/O2 ya la hizo
    // el hijo de frontera de proceso de arriba; repetirla aqui sin ese limite no ejercitaria la
    // instrumentacion real de LLVM-C.dll.
#if !defined(_WIN32)
    {
        for (const LlvmOptimizationLevel level : {LlvmOptimizationLevel::O0,
                                                  LlvmOptimizationLevel::O1,
                                                  LlvmOptimizationLevel::O2})
        {
            const std::string levelName(llvmOptimizationLevelName(level));
            std::string plainText;
            std::string observedText;
            for (const bool observed : {false, true})
            {
                std::string targetError;
                std::unique_ptr<LlvmTargetMachine> targetMachine =
                    LlvmTargetMachine::createWindowsX64(level, targetError);
                FrontendCompilation compilation;
                std::unique_ptr<LlvmLowerer> lowerer;
                std::unique_ptr<LlvmModule> module = makeInliningFixture(compilation, lowerer);
                if (module == nullptr || targetMachine == nullptr) continue;
                LlvmOptimizationObservation observation;
                std::string optimizationError;
                if (!optimizer.optimize(
                        *module, *targetMachine, level, observed ? &observation : nullptr,
                        optimizationError))
                {
                    continue;
                }
                (observed ? observedText : plainText) = module->text();
            }
            tests.expect(!plainText.empty() && plainText == observedText,
                "instrumentation leaves the optimized IR byte-identical at " + levelName);
        }
    }
#endif

    return tests.result();
}
