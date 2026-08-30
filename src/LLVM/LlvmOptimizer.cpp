#include "nativesharp/LLVM/LlvmOptimizer.h"

#include "LlvmCApi.h"

#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"

#include <string>
#include <string_view>

namespace nativesharp::llvm_backend
{
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
}

bool LlvmOptimizer::optimize(
    LlvmModule& module,
    const LlvmTargetMachine& targetMachine,
    LlvmOptimizationLevel level,
    std::string& error) const
{
    error.clear();

    const std::string_view pipeline = llvmOptimizationPipelineName(level);
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
    if (!module.verify(verificationError))
    {
        error = "refusing to optimize an invalid LLVM module: " + verificationError;
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
    llvm_api::LLVMPassBuilderOptionsSetVerifyEach(options, 1);

    llvm_api::LLVMModuleRef moduleRef = static_cast<llvm_api::LLVMModuleRef>(module.module_);
    llvm_api::LLVMTargetMachineRef targetMachineRef =
        static_cast<llvm_api::LLVMTargetMachineRef>(targetMachine.targetMachine_);

    // Bug B-001 fix -- see legalizeSehFuncletFunctionsForO0's header comment above for the full
    // story. Only -O0 needs this: -O1 and above already run mem2reg as part of their own default
    // pipeline, and running it twice would just be redundant (mem2reg is idempotent, so it is
    // harmless either way, but there is no reason to pay the extra per-function pass-manager
    // construction cost at every optimization level).
    if (level == LlvmOptimizationLevel::O0)
    {
        if (!legalizeSehFuncletFunctionsForO0(moduleRef, targetMachineRef, options, error))
        {
            llvm_api::LLVMDisposePassBuilderOptions(options);
            return false;
        }
    }

    const std::string pipelineText(pipeline);
    llvm_api::LLVMErrorRef llvmError = llvm_api::LLVMRunPasses(
        moduleRef,
        pipelineText.c_str(),
        targetMachineRef,
        options);
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
        return false;
    }

    verificationError.clear();
    if (!module.verify(verificationError))
    {
        error = "LLVM optimization produced an invalid module: " + verificationError;
        return false;
    }

    return true;
}
}
