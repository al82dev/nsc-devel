#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct InlineAssemblyOperandChild final
{
    const ast::AstNode* syntax = nullptr;
    bool addressContext = false;
};
}
