#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct OrdinaryConstructionChild final
{
    const ast::AstNode* syntax = nullptr;
};
}
