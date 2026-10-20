#pragma once

#include "HirExpressionStatementMachineCommon.h"
#include "DeconstructionDeclarationFrameStage.h"

namespace nativesharp::frontend::internal
{
struct DeconstructionDeclarationFrameState final
{
    const MethodDeconstructionBinding* binding = nullptr;
    std::unique_ptr<ProjectOopHirNode> source;
    DeconstructionDeclarationFrameStage stage =
        DeconstructionDeclarationFrameStage::Source;
    bool materializeReceiverTemporary = false;
};
}
