#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct NamespaceCallChild final
{
    const ast::AstNode* syntax = nullptr;
    bool addressContext = false;
};
}
