#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct AggregateCrossChild final
{
    const ast::AstNode* syntax = nullptr;
    bool addressContext = false;
};
}
