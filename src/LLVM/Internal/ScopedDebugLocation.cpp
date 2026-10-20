#include "ScopedDebugLocation.h"

namespace nativesharp::llvm_backend
{
ScopedDebugLocation::ScopedDebugLocation(LLVMBuilderRef builder, LLVMMetadataRef location)
    : builder_(builder)
{
    if (builder_ == nullptr || location == nullptr) return;
    previous_ = LLVMGetCurrentDebugLocation2(builder_);
    LLVMSetCurrentDebugLocation2(builder_, location);
    active_ = true;
}

ScopedDebugLocation::~ScopedDebugLocation()
{
    if (active_) LLVMSetCurrentDebugLocation2(builder_, previous_);
}
}
