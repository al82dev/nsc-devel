#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
enum class DeconstructionDeclarationFrameStage : std::uint8_t
{
    Source,
    Ready,
};
}
