#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct CoroutineValueStatementFrameState final
{
    std::unique_ptr<ProjectOopHirNode> node;
};
}
