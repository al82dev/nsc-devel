#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
enum class AtomicCoreCallFrameStage : std::uint8_t
{
    Location,
    Values,
    Ready,
};
}
