#pragma once

#include "../LlvmCApi.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

class ScopedDebugScope final
{
public:
    ScopedDebugScope(LLVMMetadataRef& slot, LLVMMetadataRef scope);
    ~ScopedDebugScope();

    ScopedDebugScope(const ScopedDebugScope&) = delete;
    ScopedDebugScope& operator=(const ScopedDebugScope&) = delete;

private:
    LLVMMetadataRef& slot_;
    LLVMMetadataRef previous_ = nullptr;
};
}
