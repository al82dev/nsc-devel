#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct OrdinaryCallChild final
{
    const ast::AstNode* syntax = nullptr;
    bool addressContext = false;
};
}
