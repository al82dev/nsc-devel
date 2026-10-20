#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct BoundCallChild final
{
    const ast::AstNode* syntax = nullptr;
    bool addressContext = false;
};
}
