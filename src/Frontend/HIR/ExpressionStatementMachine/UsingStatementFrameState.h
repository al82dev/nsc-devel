#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct UsingStatementFrameState final
{
    const MethodUsingBinding* binding = nullptr;
    std::unique_ptr<ProjectOopHirNode> node;
    std::size_t cleanupOrdinal = 0U;
};
}
