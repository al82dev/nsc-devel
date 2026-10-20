#pragma once

#include "HirExpressionStatementMachineCommon.h"
#include "AggregateCrossSlot.h"
#include "AggregateCrossStructuralFrame.h"
#include "AggregateCrossFramePhase.h"

namespace nativesharp::frontend::internal
{
struct AggregateCrossFrameState final
{
    std::unique_ptr<ProjectOopHirNode> node;
    std::vector<AggregateCrossStructuralFrame> structuralFrames;
    AggregateCrossFramePhase phase = AggregateCrossFramePhase::Advance;
    const ast::AstNode* pendingSyntax = nullptr;
    AggregateCrossSlot pendingSlot;
    std::vector<std::size_t> pendingTargetOrdinals;
    bool pendingMoveSpread = false;
    std::optional<std::size_t> pendingMoveSourceLocalOrdinal;
    std::optional<std::size_t> pendingMoveSourceParameterOrdinal;
};
}
