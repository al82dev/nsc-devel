#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct ExitValueStatementFrameState final
{
    std::unique_ptr<ProjectOopHirNode> node;
    const MethodLoopBreakBinding* loopBreakBinding = nullptr;
    bool referenceReturn = false;
    bool valueReturningCallable = false;
    std::string declaredTypeKey;
    sema::OwnershipKind declaredOwnership = sema::OwnershipKind::None;
};
}
