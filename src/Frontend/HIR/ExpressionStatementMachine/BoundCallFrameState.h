#pragma once

#include "HirExpressionStatementMachineCommon.h"
#include "BoundCallFrameStage.h"
#include "ReceiverFrameState.h"

namespace nativesharp::frontend::internal
{
struct BoundCallFrameState final
{
    const MethodCallBinding* binding = nullptr;
    const MethodExpressionTypeBinding* typeBinding = nullptr;
    ProjectOopHirModel::BoundCallTarget target;
    BoundCallFrameStage stage = BoundCallFrameStage::Ready;
    std::vector<const ast::AstNode*> argumentSyntaxes;
    std::vector<std::unique_ptr<ProjectOopHirNode>> arguments;
    ReceiverFrameState receiver;
    std::size_t nextArgument = 0U;
    bool nullConditional = false;
    bool hasOwnedTemporary = false;
};
}
