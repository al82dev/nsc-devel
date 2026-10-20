#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct MonitorCoreCallChild final
{
    const ast::AstNode* syntax = nullptr;
};
}
