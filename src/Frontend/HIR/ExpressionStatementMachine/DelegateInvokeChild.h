#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct DelegateInvokeChild final
{
    const ast::AstNode* syntax = nullptr;
};
}
