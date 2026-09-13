#include "nativesharp/LLVM/LlvmOptimizer.h"

#include "LlvmCApi.h"

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

// NSC-LIMIT-005 -- CAPTURAR LA SALIDA DE ERROR DEL PROCESO MIENTRAS CORRE `LLVMRunPasses`.
//
// POR QUE ASI Y NO DE OTRA FORMA. `LLVMPassBuilderOptionsSetDebugLogging` hace que LLVM escriba
// que pase ejecuta y sobre que, pero lo escribe por `llvm::dbgs()`, que es la salida de error del
// PROCESO. La API C no acepta un sink. De modo que o se desvia el descriptor 2 durante la llamada,
// o se entra en la API C++ -- y eso ultimo es una decision de arquitectura, no un detalle de
// implementacion.
//
// ESTA CLASE ES SOLO PARA POSIX. En Windows el producto enlaza LLVM-C.dll, que lleva su propio CRT:
// `_dup2` en nsc cambia el descriptor de nsc, no el de la DLL. Alli la captura se establece al
// CREAR el proceso hijo; ver LlvmProcessTrace.cpp y src/nsc/main.cpp.
#if !defined(_WIN32)
class StandardErrorCapture final
{
public:
    explicit StandardErrorCapture(std::string path) : path_(std::move(path))
    {
        saved_ = ::dup(2);
        if (saved_ == -1) return;
        const int file = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (file == -1) { ::close(saved_); saved_ = -1; return; }
        active_ = ::dup2(file, 2) != -1;
        ::close(file);
    }

    StandardErrorCapture(const StandardErrorCapture&) = delete;
    StandardErrorCapture& operator=(const StandardErrorCapture&) = delete;

    ~StandardErrorCapture() { restore(); }

    bool active() const noexcept { return active_; }

    void restore()
    {
        if (saved_ == -1) return;
        ::dup2(saved_, 2);
        ::close(saved_);
        saved_ = -1;
        active_ = false;
    }

    std::string text() const
    {
        std::ifstream file(path_, std::ios::binary);
        if (!file) return {};
        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    void remove() const { std::remove(path_.c_str()); }

private:
    std::string path_;
    int saved_ = -1;
    bool active_ = false;
};

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

    llvm_api::LLVMPassBuilderOptionsRef options = llvm_api::LLVMCreatePassBuilderOptions();
    if (options == nullptr)
    {
        error = "LLVMCreatePassBuilderOptions failed";
        return false;
    }
    // O0 already verifies the complete module immediately before and after the pipeline. Asking
    // LLVM to verify after every individual O0 pass repeats the same whole-module walk without
    // improving the release boundary: any invalid result is still rejected before emission. The
    // optimized pipelines retain verify-each when detailed pass diagnostics were explicitly
    // requested, because then naming the failing pass is valuable evidence.
    const bool detailedPassDiagnostics = llvmProcessTraceActive();
    llvm_api::LLVMPassBuilderOptionsSetVerifyEach(
        options, llvmVerifyEachPassRequired(level, detailedPassDiagnostics) ? 1 : 0);
    // En Windows sólo se enciende dentro del hijo cuya stderr fue fijada por CreateProcess. Hacerlo
    // en el proceso padre produciria un registro que nadie puede capturar: LLVM-C.dll lleva su
    // propio CRT. En POSIX se conserva la captura temporal dentro del proceso.
#if defined(_WIN32)
    const bool emitProcessTrace = observation != nullptr && detailedPassDiagnostics;
    const bool enableDebugLogging = emitProcessTrace;
#else
    const bool emitProcessTrace = false;
    const bool enableDebugLogging = observation != nullptr && detailedPassDiagnostics;
#endif
    if (enableDebugLogging)
    {
        llvm_api::LLVMPassBuilderOptionsSetDebugLogging(options, 1);
    }

    llvm_api::LLVMModuleRef moduleRef = static_cast<llvm_api::LLVMModuleRef>(module.module_);
    llvm_api::LLVMTargetMachineRef targetMachineRef =
        static_cast<llvm_api::LLVMTargetMachineRef>(targetMachine.targetMachine_);

    const std::string pipelineText(pipeline);
    if (observation != nullptr)
    {
        observation->pipeline = pipelineText;
        // El triple y el data layout se leen AQUI, despues de configureModuleForTarget y justo
        // antes de optimizar: son los que la API C tiene delante cuando decide.
        observation->moduleTriple = moduleString(llvm_api::LLVMGetTarget(moduleRef));
        observation->moduleDataLayout = moduleString(llvm_api::LLVMGetDataLayoutStr(moduleRef));
        observation->processTraceActive = emitProcessTrace;
    }

#if defined(_WIN32)
    if (emitProcessTrace)
    {
        std::fwrite(kLlvmProcessTraceBegin.data(), 1, kLlvmProcessTraceBegin.size(), stderr);
        std::fputc('\n', stderr);
        std::fflush(stderr);
    }
    const auto finishProcessTrace = [&]() {
        if (!emitProcessTrace) return;
        // La sonda tiene que escribirla LLVM, no nsc. Un tipo identificado evita confundirla con
        // cualquier `i32` legitimo que aparezca en el registro de pases.
        llvm_api::LLVMDumpType(llvm_api::LLVMStructCreateNamed(
            llvm_api::LLVMGetModuleContext(moduleRef),
            std::string(kLlvmStandardErrorProbe).c_str()));
        std::fwrite(kLlvmProcessTraceEnd.data(), 1, kLlvmProcessTraceEnd.size(), stderr);
        std::fputc('\n', stderr);
        std::fflush(stderr);
    };
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
            finishProcessTrace();
#endif
            llvm_api::LLVMDisposePassBuilderOptions(options);
            return false;
        }
    }
    llvm_api::LLVMErrorRef llvmError = nullptr;
#if defined(_WIN32)
    llvmError = llvm_api::LLVMRunPasses(
        moduleRef, pipelineText.c_str(), targetMachineRef, options);
    finishProcessTrace();
#else
    if (observation != nullptr)
    {
        // El fichero vive lo que dura la llamada y se borra siempre. No es un artifact: es el
        // unico sitio por el que LLVM sabe hablar. Nada de `std::tmpnam`, que MSVC marca como
        // deprecado y que ademas no dice donde escribe.
        const std::string capturePath = captureFilePath();
        if (capturePath.empty())
        {
            llvmError = llvm_api::LLVMRunPasses(
                moduleRef, pipelineText.c_str(), targetMachineRef, options);
        }
        else
        {
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
            llvmError = llvm_api::LLVMRunPasses(
                moduleRef, pipelineText.c_str(), targetMachineRef, options);
            // Y AHORA QUE ESCRIBA LLVM, todavia dentro de la ventana. Se usa un nombre imposible
            // de confundir con el IR normal; buscar `i32` daba falsos positivos.
            if (capture.active())
            {
                llvm_api::LLVMDumpType(llvm_api::LLVMStructCreateNamed(
                    llvm_api::LLVMGetModuleContext(moduleRef),
                    std::string(kLlvmStandardErrorProbe).c_str()));
                std::fflush(stderr);
            }
            // Se restaura ANTES de leer: leer con el descriptor todavia desviado dejaria cualquier
            // mensaje de la propia lectura dentro del fichero que se esta leyendo.
            capture.restore();
            std::string captured = capture.text();
            const std::size_t probe = captured.find(kLlvmCaptureProbe);
            observation->captureVerified = probe != std::string::npos;
            if (observation->captureVerified)
            {
                captured.erase(probe, kLlvmCaptureProbe.size());
            }
            const std::size_t emitted = captured.find(kLlvmStandardErrorProbe);
            observation->llvmStandardErrorCaptured = emitted != std::string::npos;
            if (observation->llvmStandardErrorCaptured)
            {
                captured.erase(emitted, kLlvmStandardErrorProbe.size());
            }
            observation->capturedBytes = captured.size();
            observeLlvmPassLog(captured, *observation);
            capture.remove();
        }
    }
    else
    {
        llvmError = llvm_api::LLVMRunPasses(
            moduleRef, pipelineText.c_str(), targetMachineRef, options);
    }
#endif
    if (observation != nullptr)
    {
        const auto elapsed = std::chrono::steady_clock::now() - started;
        observation->seconds = std::chrono::duration<double>(elapsed).count();
        observation->timeBudgetExceeded =
            observation->seconds > kLlvmOptimizationTimeBudgetSeconds;
        observeActiveLlvmOptimizationBudget(
            level, std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed));
    }
    else
    {
        observeActiveLlvmOptimizationBudget(level,
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started));
    }
    llvm_api::LLVMDisposePassBuilderOptions(options);

    if (llvmError != nullptr)
    {
        char* message = llvm_api::LLVMGetErrorMessage(llvmError);
        if (message != nullptr)
        {
            error.assign(message);
            llvm_api::LLVMDisposeErrorMessage(message);
        }
        else
        {
            error = "LLVMRunPasses failed without a diagnostic";
        }
        // El cierre del Gate 1 pide que un fallo del optimizador identifique LLVM, el pipeline, la
        // funcion/unidad y el ultimo pass observado. Sin observacion se dice lo que se sabe.
        if (observation != nullptr) error += " [" + observation->report() + "]";
        return false;
    }

    verificationError.clear();
    const bool verifiedAfter = module.verify(verificationError);
    if (observation != nullptr) observation->verifiedAfter = verifiedAfter;
    if (!verifiedAfter)
    {
        error = "LLVM optimization produced an invalid module: " + verificationError;
        if (observation != nullptr) error += " [" + observation->report() + "]";
        return false;
    }

    return true;
}
}
