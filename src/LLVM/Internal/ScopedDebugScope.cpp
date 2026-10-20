#include "ScopedDebugScope.h"

namespace nativesharp::llvm_backend
{
ScopedDebugScope::ScopedDebugScope(LLVMMetadataRef& slot, LLVMMetadataRef scope)
    : slot_(slot), previous_(slot)
{
    if (scope != nullptr) slot_ = scope;
}

ScopedDebugScope::~ScopedDebugScope()
{
    slot_ = previous_;
}
}
