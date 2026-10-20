#pragma once

#include "HirExpressionStatementMachineCommon.h"
#include "OrdinaryConstructionFrameStage.h"

namespace nativesharp::frontend::internal
{
struct OrdinaryConstructionFrameState final
{
    const ObjectConstructionBinding* binding = nullptr;
    std::unique_ptr<ProjectOopHirNode> node;
    OrdinaryConstructionFrameStage stage =
        OrdinaryConstructionFrameStage::Ready;
    std::size_t nextArgument = 0U;
    std::optional<std::size_t> expandedPackChildIndex;
    std::vector<std::size_t> fieldMemberOrdinals;
    std::size_t nextFieldInitializer = 0U;
    bool hasOwnedTemporary = false;
};
}
