#include "nativesharp/LLVM/LlvmNativeSanitizer.h"

#include "LlvmCApi.h"

#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"

namespace nativesharp::llvm_backend
{
std::string_view nativeSanitizerName(NativeSanitizerKind kind) noexcept
{
    switch (kind)
    {
    case NativeSanitizerKind::Address: return "address";
    }
    return {};
}

std::string_view nativeSanitizerPipeline(NativeSanitizerKind kind) noexcept
{
    switch (kind)
    {
    case NativeSanitizerKind::Address: return "asan";
    }
    return {};
}

std::string_view nativeSanitizerWindowsRuntime(NativeSanitizerKind kind) noexcept
{
    switch (kind)
    {
    case NativeSanitizerKind::Address: return "clang_rt.asan_dynamic-x86_64";
    }
    return {};
}

bool LlvmNativeSanitizer::instrument(
    LlvmModule& module,
    const LlvmTargetMachine& targetMachine,
    NativeSanitizerKind kind,
    std::string& error) const
{
    error.clear();
    const std::string_view pipeline = nativeSanitizerPipeline(kind);
    if (pipeline.empty())
    {
        error = "unsupported native sanitizer";
        return false;
    }

    std::string verificationError;
    if (!module.verify(verificationError))
    {
        error = "refusing to sanitize an invalid LLVM module: " + verificationError;
        return false;
    }
    if (!targetMachine.configureModuleForTarget(module, error)) return false;

    llvm_api::LLVMModuleRef moduleRef =
        static_cast<llvm_api::LLVMModuleRef>(module.module_);
    if (kind == NativeSanitizerKind::Address)
    {
        static constexpr std::string_view attributeName = "sanitize_address";
        const unsigned attributeKind = llvm_api::LLVMGetEnumAttributeKindForName(
            attributeName.data(), attributeName.size());
        if (attributeKind == 0)
        {
            error = "LLVM does not expose the sanitize_address function attribute";
            return false;
        }
        constexpr llvm_api::LLVMAttributeIndex functionAttributeIndex = ~0U;
        for (llvm_api::LLVMValueRef function = llvm_api::LLVMGetFirstFunction(moduleRef);
             function != nullptr;
             function = llvm_api::LLVMGetNextFunction(function))
        {
            if (llvm_api::LLVMIsDeclaration(function)) continue;
            llvm_api::LLVMAddAttributeAtIndex(
                function, functionAttributeIndex,
                llvm_api::LLVMCreateEnumAttribute(
                    static_cast<llvm_api::LLVMContextRef>(module.context_),
                    attributeKind, 0));
        }
    }

    llvm_api::LLVMPassBuilderOptionsRef options = llvm_api::LLVMCreatePassBuilderOptions();
    if (options == nullptr)
    {
        error = "LLVMCreatePassBuilderOptions failed for native sanitizer";
        return false;
    }
    llvm_api::LLVMPassBuilderOptionsSetVerifyEach(options, 1);
    llvm_api::LLVMErrorRef llvmError = llvm_api::LLVMRunPasses(
        moduleRef,
        std::string(pipeline).c_str(),
        static_cast<llvm_api::LLVMTargetMachineRef>(targetMachine.targetMachine_),
        options);
    llvm_api::LLVMDisposePassBuilderOptions(options);
    if (llvmError != nullptr)
    {
        char* message = llvm_api::LLVMGetErrorMessage(llvmError);
        error = message != nullptr
            ? "native sanitizer instrumentation failed: " + std::string(message)
            : "native sanitizer instrumentation failed without a diagnostic";
        if (message != nullptr) llvm_api::LLVMDisposeErrorMessage(message);
        return false;
    }
    if (!module.verify(verificationError))
    {
        error = "native sanitizer produced invalid LLVM IR: " + verificationError;
        return false;
    }
    return true;
}
}
