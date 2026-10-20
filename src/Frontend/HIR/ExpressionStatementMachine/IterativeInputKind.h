#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
enum class IterativeInputKind : std::uint8_t
{
    Expression,
    LoopBlock,
};
}
