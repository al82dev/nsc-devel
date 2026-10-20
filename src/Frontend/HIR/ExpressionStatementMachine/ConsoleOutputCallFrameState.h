#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct ConsoleOutputCallFrameState final
{
    const MethodCallBinding* binding = nullptr;
    std::uint64_t mask = 0U;
    bool hasOwnedTemporary = false;
};
}
