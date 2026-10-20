#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
enum class DelegateInvokeFrameStage : std::uint8_t
{
    Callee,
    Arguments,
    Ready,
};
}
