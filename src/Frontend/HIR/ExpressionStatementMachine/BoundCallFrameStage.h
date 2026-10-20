#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
enum class BoundCallFrameStage : std::uint8_t
{
    Receiver,
    Arguments,
    Ready,
};
}
