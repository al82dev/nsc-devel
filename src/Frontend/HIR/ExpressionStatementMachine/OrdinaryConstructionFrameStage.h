#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
enum class OrdinaryConstructionFrameStage : std::uint8_t
{
    Storage,
    Arguments,
    FieldInitializers,
    Ready,
};
}
