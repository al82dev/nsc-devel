#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct VariableDeclarationChild final
{
    const ast::AstNode* syntax = nullptr;
    bool addressContext = false;
};
}
