#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct ForeachStatementFrameState final
{
    std::unique_ptr<ProjectOopHirNode> node;
    const ast::AstNode* chosenCandidate = nullptr;
};
}
