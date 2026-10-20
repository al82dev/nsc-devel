#include "nativesharp/LLVM/LlvmOptimizer.h"

#include "LlvmCApi.h"
#include "Internal/StandardErrorCapture.h"

#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmProcessTrace.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if !defined(_WIN32)
#  include <fcntl.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif

namespace nativesharp::llvm_backend
{
bool llvmVerifyEachPassRequired(
    LlvmOptimizationLevel level, bool detailedPassDiagnostics) noexcept
{
    return level != LlvmOptimizationLevel::O0 && detailedPassDiagnostics;
}

AutomaticInliningConfiguration llvmAutomaticInliningConfiguration(
    LlvmOptimizationLevel level) noexcept
{
    switch (level)
    {
    case LlvmOptimizationLevel::O0:
        return {AutomaticInliningCostModel::Disabled, "default<O0>"};
    case LlvmOptimizationLevel::O1:
        return {AutomaticInliningCostModel::Conservative, "default<O1>"};
    case LlvmOptimizationLevel::O2:
        return {AutomaticInliningCostModel::Balanced, "default<O2>"};
    case LlvmOptimizationLevel::O3:
        return {AutomaticInliningCostModel::Aggressive, "default<O3>"};
    }
    return {AutomaticInliningCostModel::Disabled, {}};
}

namespace
{
// Bug B-001 fix (EH+RAII reuse-after-catch SIGSEGV/access-violation crash) -- see
// BUGS_ENCONTRADOS/bug_try_catch_reuso_objeto_tras_catch.md for the original report and the
// project's "30_..." docs for the full root-cause writeup. Summary of what was actually found,
// for whoever next touches this function:
//
// LLVM's "default<O0>" new-pass-manager pipeline (used for every -O0 build) intentionally never
// runs mem2reg/SROA -- every local stays a real, un-promoted `alloca` + explicit load/store, on
// purpose, so that -O0 output stays maximally debug-friendly (one stable stack slot per local
// for the whole function, matching what a future PDB/CodeView backend -- see the project's
// roadmap P1 item on debug info -- will want to describe). That is completely fine for ordinary
// functions.
//
// It stops being fine for a function that uses Windows x64 SEH funclets (any method containing
// a real try/catch or try/finally, recognizable here by LLVMHasPersonalityFn): confirmed via a
// real Wine PE execution bisection (five isolated repros, see the doc) that LLVM's WinEHPrepare/
// SelectionDAG lowering for `catchswitch`/`catchpad`/`cleanuppad` at CodeGenOpt::None -- and ONLY
// at that opt level, -O1 and -O2 were both confirmed clean on the exact same IR shape -- produces
// a function whose machine code corrupts its own frame the first time a *later*, unrelated
// exception-protected call executes after a real catch has run and returned via `catchret`,
// causing a genuine STATUS_ACCESS_VIOLATION (0xC0000005, confirmed with winedbg) on that later
// call -- not on anything related to the exception itself. The trigger needs nothing more exotic
// than "a real throw, caught once, followed anywhere later in the same function by another call
// that also needs unwind protection" -- no second try/catch, no finally, and no second RAII
// object are required; a single plain method call after the handled catch is enough.
//
// The fix that was actually verified to remove the crash (bisected against the O0/O1 pipeline
// difference: inserting exactly one `mem2reg` pass ahead of "default<O0>" makes the crash
// disappear, with zero effect on -O1/-O2 which already include it): promote that one function's
// allocas to SSA registers *before* WinEHPrepare ever sees them, exactly as -O1 already does.
// Doing this UNCONDITIONALLY for every -O0 function (a blanket "always mem2reg at O0") also
// passed the full test suite with zero regressions during diagnosis, but was deliberately NOT
// kept as the shipped fix: it would silently take away -O0's real-stack-slot-per-local property
// from every ordinary function too, not just the handful that actually need the workaround, and
// there is no reason yet to pay that cost project-wide for a bug that only exists in
// SEH-funclet-bearing functions. Scoping the promotion to exactly those functions (via
// LLVMRunPassesOnFunction, LLVM's per-function C API pipeline entry point) keeps -O0 unchanged
// for every function that never touches try/catch/finally.
bool legalizeSehFuncletFunctionsForO0(
    llvm_api::LLVMModuleRef module,
    llvm_api::LLVMTargetMachineRef targetMachine,
    llvm_api::LLVMPassBuilderOptionsRef options,
    std::string& error)
{
    for (llvm_api::LLVMValueRef function = llvm_api::LLVMGetFirstFunction(module);
         function != nullptr;
         function = llvm_api::LLVMGetNextFunction(function))
    {
        // A declaration (an imported runtime/Win32 function with no body of its own, e.g.
        // RaiseException) can carry no funclets and has nothing for mem2reg to legalize.
        if (llvm_api::LLVMIsDeclaration(function)) continue;
        if (!llvm_api::LLVMHasPersonalityFn(function)) continue;

        llvm_api::LLVMErrorRef functionError = llvm_api::LLVMRunPassesOnFunction(
            function, "mem2reg", targetMachine, options);
        if (functionError != nullptr)
        {
            char* message = llvm_api::LLVMGetErrorMessage(functionError);
            error = (message != nullptr)
                ? ("Bug B-001 O0 SEH legalization (mem2reg) failed: " + std::string(message))
                : "Bug B-001 O0 SEH legalization (mem2reg) failed without a diagnostic";
            if (message != nullptr) llvm_api::LLVMDisposeErrorMessage(message);
            return false;
        }
    }
    return true;
}

bool hasSehFuncletFunctions(llvm_api::LLVMModuleRef module)
{
    for (llvm_api::LLVMValueRef function = llvm_api::LLVMGetFirstFunction(module);
         function != nullptr;
         function = llvm_api::LLVMGetNextFunction(function))
    {
        if (!llvm_api::LLVMIsDeclaration(function) &&
            llvm_api::LLVMHasPersonalityFn(function)) return true;
    }
    return false;
}

#if !defined(_WIN32)
// EL IDENTIFICADOR DEL PROCESO, para que el nombre del fichero de captura sea unico ENTRE
// PROCESOS y no solo dentro de uno. El contador atomico de abajo separa dos capturas del mismo
// nsc.exe; sin el pid, dos nsc.exe simultaneos --que es exactamente lo que hace `ctest -j 16`--
// abririan el MISMO fichero con `_O_TRUNC` y cada uno leeria pases del otro. Un reporte de
// diagnostico que mezcla dos compilaciones es peor que no tener reporte.
long long currentProcessId()
{
    return static_cast<long long>(::getpid());
}

// Un nombre de fichero temporal propio, sin `std::tmpnam`. Si el sistema no da un directorio
// temporal se devuelve vacio y la optimizacion sigue SIN captura: observar no puede impedir
// compilar.
std::string captureFilePath()
{
    std::error_code failure;
    const std::filesystem::path directory = std::filesystem::temp_directory_path(failure);
    if (failure) return {};
    static std::atomic<unsigned long long> counter{0};
    std::ostringstream name;
    name << "nsc-llvm-opt-" << currentProcessId() << '-' << counter.fetch_add(1) << ".log";
    return (directory / name.str()).string();
}
#endif

std::string moduleString(const char* text)
{
    return text != nullptr ? std::string(text) : std::string();
}

#if defined(_WIN32)
void beginLlvmProcessTrace(bool emitProcessTrace)
{
    if (!emitProcessTrace) return;
    std::fwrite(kLlvmProcessTraceBegin.data(), 1, kLlvmProcessTraceBegin.size(), stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);
}

void finishLlvmProcessTrace(llvm_api::LLVMModuleRef module, bool emitProcessTrace)
{
    if (!emitProcessTrace) return;
    // La sonda tiene que escribirla LLVM, no nsc. Un tipo identificado evita confundirla con
    // cualquier `i32` legitimo que aparezca en el registro de pases.
    llvm_api::LLVMDumpType(llvm_api::LLVMStructCreateNamed(
        llvm_api::LLVMGetModuleContext(module), std::string(kLlvmStandardErrorProbe).c_str()));
    std::fwrite(kLlvmProcessTraceEnd.data(), 1, kLlvmProcessTraceEnd.size(), stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);
}
#endif

llvm_api::LLVMErrorRef runOptimizationPassPipeline(
    llvm_api::LLVMModuleRef module,
    llvm_api::LLVMTargetMachineRef targetMachine,
    llvm_api::LLVMPassBuilderOptionsRef options,
    std::string_view pipeline,
    bool preparedO0FastPath,
    bool emitProcessTrace,
    LlvmOptimizationObservation* observation)
{
#if defined(_WIN32)
    (void)observation;
    if (preparedO0FastPath) return nullptr;
    llvm_api::LLVMErrorRef error = llvm_api::LLVMRunPasses(
        module, std::string(pipeline).c_str(), targetMachine, options);
    finishLlvmProcessTrace(module, emitProcessTrace);
    return error;
#else
    (void)emitProcessTrace;
    if (preparedO0FastPath) return nullptr;
    if (observation == nullptr)
        return llvm_api::LLVMRunPasses(
            module, std::string(pipeline).c_str(), targetMachine, options);
    // El fichero vive lo que dura la llamada y se borra siempre. No es un artifact: es el
    // unico sitio por el que LLVM sabe hablar. Nada de `std::tmpnam`, que MSVC marca como
    // deprecado y que ademas no dice donde escribe.
    const std::string capturePath = captureFilePath();
    if (capturePath.empty())
        return llvm_api::LLVMRunPasses(
            module, std::string(pipeline).c_str(), targetMachine, options);
    StandardErrorCapture capture(capturePath);
    // COMPROBAR LA CAPTURA CON UNA MARCA PROPIA, antes de que LLVM diga nada.
    //
    // Sin esto, `observedPassCount == 0` no distingue «LLVM no escribio» de «no se
    // escucho», y son dos problemas opuestos: el primero no tiene arreglo desde aqui y el
    // segundo si. La marca se escribe por `stderr` --el mismo descriptor 2 que se acaba de
    // desviar-- y se vacia a mano, porque un buffer sin vaciar daria un falso negativo.
    if (capture.active())
    {
        std::fputs(std::string(kLlvmCaptureProbe).c_str(), stderr);
        std::fflush(stderr);
    }
    llvm_api::LLVMErrorRef error = llvm_api::LLVMRunPasses(
        module, std::string(pipeline).c_str(), targetMachine, options);
    // Y AHORA QUE ESCRIBA LLVM, todavia dentro de la ventana. Se usa un nombre imposible
    // de confundir con el IR normal; buscar `i32` daba falsos positivos.
    if (capture.active())
    {
        llvm_api::LLVMDumpType(llvm_api::LLVMStructCreateNamed(
            llvm_api::LLVMGetModuleContext(module), std::string(kLlvmStandardErrorProbe).c_str()));
        std::fflush(stderr);
    }
    // Se restaura ANTES de leer: leer con el descriptor todavia desviado dejaria cualquier
    // mensaje de la propia lectura dentro del fichero que se esta leyendo.
    capture.restore();
    std::string captured = capture.text();
    const std::size_t probe = captured.find(kLlvmCaptureProbe);
    observation->captureVerified = probe != std::string::npos;
    if (observation->captureVerified) captured.erase(probe, kLlvmCaptureProbe.size());
    const std::size_t emitted = captured.find(kLlvmStandardErrorProbe);
    observation->llvmStandardErrorCaptured = emitted != std::string::npos;
    if (observation->llvmStandardErrorCaptured)
        captured.erase(emitted, kLlvmStandardErrorProbe.size());
    observation->capturedBytes = captured.size();
    observeLlvmPassLog(captured, *observation);
    capture.remove();
    return error;
#endif
}

llvm_api::LLVMPassBuilderOptionsRef createOptimizationPassOptions(
    LlvmOptimizationLevel level,
    bool detailedPassDiagnostics,
    bool preparedO0FastPath,
    LlvmOptimizationObservation* observation,
    llvm_api::LLVMModuleRef module,
    std::string_view pipeline,
    bool& emitProcessTrace,
    std::string& pipelineText,
    std::string& error)
{
    llvm_api::LLVMPassBuilderOptionsRef options = llvm_api::LLVMCreatePassBuilderOptions();
    if (options == nullptr)
    {
        error = "LLVMCreatePassBuilderOptions failed";
        return nullptr;
    }
    // O0 already verifies the complete module immediately before and after the pipeline. Asking
    // LLVM to verify after every individual O0 pass repeats the same whole-module walk without
    // improving the release boundary: any invalid result is still rejected before emission. The
    // optimized pipelines retain verify-each when detailed pass diagnostics were explicitly
    // requested, because then naming the failing pass is valuable evidence.
    llvm_api::LLVMPassBuilderOptionsSetVerifyEach(
        options, llvmVerifyEachPassRequired(level, detailedPassDiagnostics) ? 1 : 0);
    // En Windows sólo se enciende dentro del hijo cuya stderr fue fijada por CreateProcess. Hacerlo
    // en el proceso padre produciria un registro que nadie puede capturar: LLVM-C.dll lleva su
    // propio CRT. En POSIX se conserva la captura temporal dentro del proceso.
#if defined(_WIN32)
    emitProcessTrace = observation != nullptr && detailedPassDiagnostics && !preparedO0FastPath;
    const bool enableDebugLogging = emitProcessTrace;
#else
    emitProcessTrace = false;
    const bool enableDebugLogging = observation != nullptr && detailedPassDiagnostics;
#endif
    if (enableDebugLogging) llvm_api::LLVMPassBuilderOptionsSetDebugLogging(options, 1);
    pipelineText = preparedO0FastPath ? "nsc-o0-seh-mem2reg" : std::string(pipeline);
    if (observation != nullptr)
    {
        observation->pipeline = pipelineText;
        // El triple y el data layout se leen AQUI, despues de configureModuleForTarget y justo
        // antes de optimizar: son los que la API C tiene delante cuando decide.
        observation->moduleTriple = moduleString(llvm_api::LLVMGetTarget(module));
        observation->moduleDataLayout = moduleString(llvm_api::LLVMGetDataLayoutStr(module));
        observation->processTraceActive = emitProcessTrace;
    }
    return options;
}

void finishOptimizationTiming(
    std::chrono::steady_clock::time_point started,
    LlvmOptimizationLevel level,
    LlvmOptimizationObservation* observation)
{
    const auto elapsed = std::chrono::steady_clock::now() - started;
    if (observation != nullptr)
    {
        observation->seconds = std::chrono::duration<double>(elapsed).count();
        observation->timeBudgetExceeded =
            observation->seconds > kLlvmOptimizationTimeBudgetSeconds;
    }
    observeActiveLlvmOptimizationBudget(
        level, std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed));
}

bool consumeOptimizationError(
    llvm_api::LLVMErrorRef llvmError,
    LlvmOptimizationObservation* observation,
    std::string& error)
{
    if (llvmError == nullptr) return true;
    char* message = llvm_api::LLVMGetErrorMessage(llvmError);
    if (message != nullptr)
    {
        error.assign(message);
        llvm_api::LLVMDisposeErrorMessage(message);
    }
    else error = "LLVMRunPasses failed without a diagnostic";
    // El cierre del Gate 1 pide que un fallo del optimizador identifique LLVM, el pipeline, la
    // funcion/unidad y el ultimo pass observado. Sin observacion se dice lo que se sabe.
    if (observation != nullptr) error += " [" + observation->report() + "]";
    return false;
}
}

bool LlvmOptimizer::optimize(
    LlvmModule& module,
    const LlvmTargetMachine& targetMachine,
    LlvmOptimizationLevel level,
    std::string& error) const
{
    return optimize(module, targetMachine, level, nullptr, error);
}

bool LlvmOptimizer::optimize(
    LlvmModule& module,
    const LlvmTargetMachine& targetMachine,
    LlvmOptimizationLevel level,
    LlvmOptimizationObservation* observation,
    std::string& error) const
{
    return optimizeImpl(module, targetMachine, level, observation, true, true, error);
}

bool LlvmOptimizer::optimizePrepared(
    LlvmModule& module,
    const LlvmTargetMachine& targetMachine,
    LlvmOptimizationLevel level,
    bool mandatoryInliningRequired,
    LlvmOptimizationObservation* observation,
    std::string& error) const
{
    return optimizeImpl(module, targetMachine, level, observation, false,
        mandatoryInliningRequired, error);
}

bool LlvmOptimizer::optimizeImpl(
    LlvmModule& module,
    const LlvmTargetMachine& targetMachine,
    LlvmOptimizationLevel level,
    LlvmOptimizationObservation* observation,
    bool prepareModule,
    bool mandatoryInliningRequired,
    std::string& error) const
{
    error.clear();
    if (observation != nullptr)
    {
        *observation = LlvmOptimizationObservation{};
        observation->level = level;
        llvm_api::LLVMGetVersion(
            &observation->llvmMajor, &observation->llvmMinor, &observation->llvmPatch);
    }

    const AutomaticInliningConfiguration automaticInlining =
        llvmAutomaticInliningConfiguration(level);
    const std::string_view pipeline = automaticInlining.pipeline;
    if (pipeline.empty())
    {
        error = "unsupported LLVM optimization level";
        return false;
    }
    if (targetMachine.optimizationLevel() != level)
    {
        error = "LLVM optimization level does not match the TargetMachine optimization level";
        return false;
    }

    if (!prepareOptimizationModule(
            module, targetMachine, prepareModule, observation, error)) return false;

    llvm_api::LLVMModuleRef moduleRef = static_cast<llvm_api::LLVMModuleRef>(module.module_);
    llvm_api::LLVMTargetMachineRef targetMachineRef =
        static_cast<llvm_api::LLVMTargetMachineRef>(targetMachine.targetMachine_);
    const bool preparedO0FastPath = !prepareModule &&
        level == LlvmOptimizationLevel::O0 && !mandatoryInliningRequired;
    const bool requiresSehLegalization =
        preparedO0FastPath && hasSehFuncletFunctions(moduleRef);
    if (preparedO0FastPath && !requiresSehLegalization)
    {
        // The lowerer already verified this module and target configuration only attaches the
        // triple/data layout. With no required inlining and no Win64 funclet workaround there is
        // no correctness pass to run at O0, so enter code generation directly.
        if (observation != nullptr)
        {
            observation->pipeline = "nsc-o0-direct-emission";
            observation->moduleTriple = moduleString(llvm_api::LLVMGetTarget(moduleRef));
            observation->moduleDataLayout = moduleString(llvm_api::LLVMGetDataLayoutStr(moduleRef));
            observation->verifiedAfter = true;
        }
        return true;
    }

    const bool detailedPassDiagnostics = llvmProcessTraceActive();
    bool emitProcessTrace = false;
    std::string pipelineText;
    llvm_api::LLVMPassBuilderOptionsRef options = createOptimizationPassOptions(
        level, detailedPassDiagnostics, preparedO0FastPath, observation, moduleRef,
        pipeline, emitProcessTrace, pipelineText, error);
    if (options == nullptr) return false;

#if defined(_WIN32)
    beginLlvmProcessTrace(emitProcessTrace);
#endif

    const auto started = std::chrono::steady_clock::now();

    // Bug B-001 fix -- see legalizeSehFuncletFunctionsForO0's header comment above for the full
    // story. Only -O0 needs this: -O1 and above already run mem2reg as part of their own default
    // pipeline, and running it twice would just be redundant (mem2reg is idempotent, so it is
    // harmless either way, but there is no reason to pay the extra per-function pass-manager
    // construction cost at every optimization level).
    if (level == LlvmOptimizationLevel::O0)
    {
        if (!legalizeSehFuncletFunctionsForO0(moduleRef, targetMachineRef, options, error))
        {
#if defined(_WIN32)
            finishLlvmProcessTrace(moduleRef, emitProcessTrace);
#endif
            llvm_api::LLVMDisposePassBuilderOptions(options);
            return false;
        }
    }
    llvm_api::LLVMErrorRef llvmError = runOptimizationPassPipeline(
        moduleRef, targetMachineRef, options, pipelineText,
        preparedO0FastPath, emitProcessTrace, observation);
    finishOptimizationTiming(started, level, observation);
    llvm_api::LLVMDisposePassBuilderOptions(options);
    if (!consumeOptimizationError(llvmError, observation, error)) return false;
    return verifyOptimizedModule(module, observation, error);
}

bool LlvmOptimizer::prepareOptimizationModule(
    LlvmModule& module,
    const LlvmTargetMachine& targetMachine,
    bool prepareModule,
    LlvmOptimizationObservation* observation,
    std::string& error) const
{
    if (!prepareModule)
    {
        if (observation != nullptr) observation->verifiedBefore = true;
        return true;
    }
    std::string verificationError;
    const bool verifiedBefore = module.verify(verificationError);
    if (observation != nullptr) observation->verifiedBefore = verifiedBefore;
    if (!verifiedBefore)
    {
        error = "refusing to optimize an invalid LLVM module: " + verificationError;
        if (observation != nullptr) error += " [" + observation->report() + "]";
        return false;
    }
    std::string configurationError;
    if (!targetMachine.configureModuleForTarget(module, configurationError))
    {
        error = configurationError;
        return false;
    }
    return true;
}

bool LlvmOptimizer::verifyOptimizedModule(
    LlvmModule& module,
    LlvmOptimizationObservation* observation,
    std::string& error) const
{
    std::string verificationError;
    const bool verifiedAfter = module.verify(verificationError);
    if (observation != nullptr) observation->verifiedAfter = verifiedAfter;
    if (verifiedAfter) return true;
    error = "LLVM optimization produced an invalid module: " + verificationError;
    if (observation != nullptr) error += " [" + observation->report() + "]";
    return false;
}
}
