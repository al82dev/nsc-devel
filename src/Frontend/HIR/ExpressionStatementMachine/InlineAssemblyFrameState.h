#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct InlineAssemblyFrameState final
{
    std::unique_ptr<ProjectOopHirNode> node;
    const ast::AstNode* pendingOperand = nullptr;
    std::size_t nextRecord = 0U;
};
}
