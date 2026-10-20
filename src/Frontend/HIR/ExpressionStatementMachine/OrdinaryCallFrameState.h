#pragma once

#include "HirExpressionStatementMachineCommon.h"
#include "ReceiverFrameState.h"
#include "OrdinaryCallFrameStage.h"

namespace nativesharp::frontend::internal
{
struct OrdinaryCallFrameState final
{
    const MethodCallBinding* binding = nullptr;
    std::unique_ptr<ProjectOopHirNode> node;
    ReceiverFrameState receiver;
    OrdinaryCallFrameStage stage = OrdinaryCallFrameStage::Ready;
    std::size_t selectedMemberOrdinal = 0U;
    std::size_t nextArgument = 0U;
    std::size_t conversionOffset = 0U;
    std::optional<std::size_t> expandedPackChildIndex;
    bool forwardedReceiver = false;
    bool receiverNeedsAddress = false;
    bool hasOwnedTemporary = false;
};
}
