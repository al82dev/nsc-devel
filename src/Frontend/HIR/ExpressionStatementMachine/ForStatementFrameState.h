#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct ForStatementFrameState final
{
    std::unique_ptr<ProjectOopHirNode> node;
};
}
