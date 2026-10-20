#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
enum class OrdinaryCallFrameStage : std::uint8_t
{
    Receiver,
    Arguments,
    Ready,
};
}
