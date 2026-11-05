#include "nativesharp/LLVM/LlvmModule.h"

#include "nativesharp/LLVM/LlvmVersion.h"

#include "LlvmCApi.h"

namespace nativesharp::llvm_backend
{
LlvmModule::LlvmModule(void* context, void* module) noexcept
    : context_(context),
      module_(module)
{
}

LlvmModule::~LlvmModule()
{
    if (module_ != nullptr)
    {
        llvm_api::LLVMDisposeModule(static_cast<llvm_api::LLVMModuleRef>(module_));
    }
    if (context_ != nullptr)
    {
        llvm_api::LLVMContextDispose(static_cast<llvm_api::LLVMContextRef>(context_));
    }
}

std::unique_ptr<LlvmModule> LlvmModule::parseText(std::string_view ir, std::string& error)
{
    error.clear();
    if (!verifyLoadedLlvmVersion(error)) return nullptr;
    auto context = llvm_api::LLVMContextCreate();
    if (!context) { error = "cannot create LLVM context"; return nullptr; }
    auto buffer = llvm_api::LLVMCreateMemoryBufferWithMemoryRangeCopy(ir.data(), ir.size(), "nsc.cpp.ir");
    if (!buffer)
    {
        llvm_api::LLVMContextDispose(context);
        error = "cannot create LLVM IR buffer";
        return nullptr;
    }
    llvm_api::LLVMModuleRef module = nullptr;
    char* message = nullptr;
    // LLVMParseIRInContext consumes the memory buffer on success and failure.
    const int failed = llvm_api::LLVMParseIRInContext(context, buffer, &module, &message);
    if (message) { error.assign(message); llvm_api::LLVMDisposeMessage(message); }
    if (failed || !module)
    {
        if (module) llvm_api::LLVMDisposeModule(module);
        llvm_api::LLVMContextDispose(context);
        if (error.empty()) error = "cannot parse LLVM IR";
        return nullptr;
    }
    auto result = std::unique_ptr<LlvmModule>(new LlvmModule(context, module));
    if (!result->verify(error)) return nullptr;
    return result;
}

bool LlvmModule::verify(std::string& error) const
{
    error.clear();
    if (!verifyLoadedLlvmVersion(error))
    {
        return false;
    }
    if (module_ == nullptr)
    {
        error = "LLVM module is null";
        return false;
    }

    char* message = nullptr;
    const int broken = llvm_api::LLVMVerifyModule(
        static_cast<llvm_api::LLVMModuleRef>(module_),
        llvm_api::LLVMReturnStatusAction,
        &message);
    if (message != nullptr)
    {
        error.assign(message);
        llvm_api::LLVMDisposeMessage(message);
    }
    return broken == 0;
}

std::string LlvmModule::text() const
{
    if (module_ == nullptr)
    {
        return {};
    }

    char* message = llvm_api::LLVMPrintModuleToString(
        static_cast<llvm_api::LLVMModuleRef>(module_));
    if (message == nullptr)
    {
        return {};
    }

    std::string result(message);
    llvm_api::LLVMDisposeMessage(message);
    return result;
}
}
