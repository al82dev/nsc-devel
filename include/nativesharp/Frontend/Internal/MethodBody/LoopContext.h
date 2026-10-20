#pragma once

struct LoopContext final
{
    std::size_t targetScopeOrdinal = 0;
    std::size_t breakCleanupDepth = 0;
    std::size_t continueCleanupDepth = 0;
    std::vector<OwnershipFlowSnapshot> breakStates;
    std::vector<OwnershipFlowSnapshot> continueStates;
    // A switch pushes a context too, because `break` inside a switch leaves the SWITCH. But it
    // is not a `continue` target: `continue` inside a switch inside a loop continues the loop,
    // so analyzeLoopExit walks past every frame with this flag when it looks for one. Without
    // the flag a `continue` in a `switch` in a `while` would either be rejected or, worse,
    // bound to a jump target that does not exist.
    bool isSwitch = false;
    const ast::AstNode* syntax = nullptr;
    std::string label;
    bool isExpression = false;
#include "nativesharp/Frontend/Internal/MethodBody/BreakValue.h"
    std::vector<BreakValue> breakValues;
};
