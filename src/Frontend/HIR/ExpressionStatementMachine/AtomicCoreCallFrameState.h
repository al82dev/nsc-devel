#pragma once

#include "HirExpressionStatementMachineCommon.h"
#include "AtomicCoreCallFrameStage.h"

namespace nativesharp::frontend::internal
{
struct AtomicCoreCallFrameState final
{
    const MethodCallBinding* binding = nullptr;
    HirAtomicCallPlan plan{};
    std::unique_ptr<ProjectOopHirNode> node;
    const ast::AstNode* locationSyntax = nullptr;
    AtomicCoreCallFrameStage stage = AtomicCoreCallFrameStage::Location;
    std::size_t argumentOffset = 0U;
    std::size_t nextValue = 0U;
    bool explicitLocation = false;
};
}
