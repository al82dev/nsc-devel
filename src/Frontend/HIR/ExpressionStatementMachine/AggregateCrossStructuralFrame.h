#pragma once

#include "HirExpressionStatementMachineCommon.h"
#include "AggregateCrossSlot.h"

namespace nativesharp::frontend::internal
{
struct AggregateCrossStructuralFrame final
{
    const ast::AstNode* syntax = nullptr;
    std::string targetTypeKey;
    std::uint64_t baseOffset = 0U;
    std::vector<AggregateCrossSlot> slots;
    const MethodAggregateInitializerBinding* binding = nullptr;
    std::size_t entryIndex = 0U;
    bool initialized = false;
};
}
