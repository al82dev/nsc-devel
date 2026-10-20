#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
enum class AllocatorConstructionFrameStage : std::uint8_t
{
    Receiver,
    Arguments,
    Ready,
};
}
