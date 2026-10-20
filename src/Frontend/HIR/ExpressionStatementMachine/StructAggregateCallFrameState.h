#pragma once

#include "HirExpressionStatementMachineCommon.h"
#include "StructAggregateCallFrameStage.h"

namespace nativesharp::frontend::internal
{
struct StructAggregateCallFrameState final
{
    const ast::AstNode* receiverSyntax = nullptr;
    std::unique_ptr<ProjectOopHirNode> node;
    StructAggregateCallFrameStage stage = StructAggregateCallFrameStage::Receiver;
    bool equals = false;
};
}
