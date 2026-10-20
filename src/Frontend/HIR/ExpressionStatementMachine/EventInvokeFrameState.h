#pragma once

#include "HirExpressionStatementMachineCommon.h"
#include "ReceiverFrameState.h"
#include "EventInvokeFrameStage.h"

namespace nativesharp::frontend::internal
{
struct EventInvokeFrameState final
{
    const MethodEventBinding* binding = nullptr;
    std::unique_ptr<ProjectOopHirNode> node;
    ReceiverFrameState receiver;
    EventInvokeFrameStage stage = EventInvokeFrameStage::Ready;
    std::size_t nextArgument = 0U;
};
}
