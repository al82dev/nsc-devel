#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
enum class LoopBlockAdvance : std::uint8_t
{
    Descend,
    Complete,
    Error,
};
}
