#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
enum class AggregateCrossFramePhase : std::uint8_t
{
    Advance,
    AwaitWithSource,
    AwaitSpreadSource,
    AwaitValue,
    Ready,
};
}
