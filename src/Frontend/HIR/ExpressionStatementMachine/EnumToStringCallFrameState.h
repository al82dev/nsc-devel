#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct EnumToStringCallFrameState final
{
    const ast::AstNode* receiverSyntax = nullptr;
    std::vector<std::pair<std::int64_t, std::string>> enumerators;
};
}
