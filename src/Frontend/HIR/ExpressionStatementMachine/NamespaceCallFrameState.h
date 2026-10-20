#pragma once

#include "HirExpressionStatementMachineCommon.h"
#include "NamespaceCallFrameStage.h"

namespace nativesharp::frontend::internal
{
struct NamespaceCallFrameState final
{
    const NamespaceFunctionCallBinding* binding = nullptr;
    const NamespaceFunctionSymbol* target = nullptr;
    std::unique_ptr<ProjectOopHirNode> node;
    ProjectOopHirNode* expandedPack = nullptr;
    NamespaceCallFrameStage stage = NamespaceCallFrameStage::Ready;
    std::size_t targetCallableOrdinal = 0U;
    std::size_t receiverParameterOrdinal = 0U;
    std::size_t nextArgument = 0U;
    std::size_t argumentOffset = 0U;
    bool receiverNeedsAddress = false;
    bool hasOwnedTemporary = false;
};
}
