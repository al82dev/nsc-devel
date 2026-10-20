#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
enum class StructAggregateCallFrameStage : std::uint8_t
{
    Receiver,
    Argument,
    Ready,
};
}
