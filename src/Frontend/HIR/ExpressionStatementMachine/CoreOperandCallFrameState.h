#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct CoreOperandCallFrameState final
{
    std::unique_ptr<ProjectOopHirNode> node;
    const MethodCallBinding* binding = nullptr;
    std::vector<const ast::AstNode*> operandSyntaxes;
    std::vector<bool> operandAddressContexts;
    std::size_t nextOperand = 0U;
    bool appliesConversions = false;
};
}
