#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
enum class VariableDeclarationFrameMode : std::uint8_t
{
    NoChild,
    Initializer,
    VlaSingle,
    VlaExtents,
    Ready,
};
}
