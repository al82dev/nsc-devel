#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct TryStatementFrameState final
{
    std::unique_ptr<ProjectOopHirNode> node;
    std::unique_ptr<ProjectOopHirNode> regionNode;
    const ast::AstNode* catchSyntax = nullptr;
    std::optional<std::size_t> cleanupOrdinal;
    std::size_t nextCatch = 0U;
    std::size_t exceptionHandlerDepth = 0U;
    bool handlerActive = false;
};
}
