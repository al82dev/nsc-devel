#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
enum class EventInvokeFrameStage : std::uint8_t
{
    Receiver,
    Arguments,
    Ready,
};
}
