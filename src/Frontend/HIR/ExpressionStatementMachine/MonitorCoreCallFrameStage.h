#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
enum class MonitorCoreCallFrameStage : std::uint8_t
{
    Receiver,
    Timeout,
    Ready,
};
}
