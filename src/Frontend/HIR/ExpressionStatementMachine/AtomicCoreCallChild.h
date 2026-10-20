#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct AtomicCoreCallChild final
{
    const ast::AstNode* syntax = nullptr;
};
}
