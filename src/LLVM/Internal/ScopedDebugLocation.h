#pragma once

#include "../LlvmCApi.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

class ScopedDebugLocation final
{
public:
    ScopedDebugLocation(LLVMBuilderRef builder, LLVMMetadataRef location);
    ~ScopedDebugLocation();

    ScopedDebugLocation(const ScopedDebugLocation&) = delete;
    ScopedDebugLocation& operator=(const ScopedDebugLocation&) = delete;

private:
    LLVMBuilderRef builder_ = nullptr;
    LLVMMetadataRef previous_ = nullptr;
    bool active_ = false;
};
}
