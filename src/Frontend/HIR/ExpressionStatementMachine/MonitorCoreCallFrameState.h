#pragma once

#include "HirExpressionStatementMachineCommon.h"
#include "MonitorCoreCallFrameStage.h"

namespace nativesharp::frontend::internal
{
struct MonitorCoreCallFrameState final
{
    const MethodCallBinding* binding = nullptr;
    HirMonitorCallPlan plan{};
    std::unique_ptr<ProjectOopHirNode> node;
    std::unique_ptr<ProjectOopHirNode> receiver;
    const ast::AstNode* timeoutSyntax = nullptr;
    MonitorCoreCallFrameStage stage = MonitorCoreCallFrameStage::Receiver;
};
}
