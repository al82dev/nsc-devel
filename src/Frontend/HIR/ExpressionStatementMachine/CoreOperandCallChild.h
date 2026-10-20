#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct CoreOperandCallChild final
{
    const ast::AstNode* syntax = nullptr;
    bool addressContext = false;
};
}
