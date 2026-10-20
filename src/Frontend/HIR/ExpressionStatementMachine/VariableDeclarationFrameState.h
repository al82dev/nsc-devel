#pragma once

#include "HirExpressionStatementMachineCommon.h"
#include "VariableDeclarationFrameMode.h"

namespace nativesharp::frontend::internal
{
struct VariableDeclarationFrameState final
{
    const MethodLocalSymbol* local = nullptr;
    std::unique_ptr<ProjectOopHirNode> node;
    std::unique_ptr<ProjectOopHirNode> vlaBlock;
    std::vector<std::unique_ptr<ProjectOopHirNode>> allocationCounts;
    VariableDeclarationFrameMode mode =
        VariableDeclarationFrameMode::NoChild;
    std::size_t nextExtent = 0U;
};
}
