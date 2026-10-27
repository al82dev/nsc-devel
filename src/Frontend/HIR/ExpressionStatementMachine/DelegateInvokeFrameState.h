#pragma once

#include "HirExpressionStatementMachineCommon.h"
#include "DelegateInvokeFrameStage.h"

namespace nativesharp::frontend::internal
{
struct DelegateInvokeFrameState final
{
    const MethodDelegateBinding* binding = nullptr;
    std::unique_ptr<ProjectOopHirNode> node;
    DelegateInvokeFrameStage stage = DelegateInvokeFrameStage::Ready;
    std::size_t nextArgument = 0U;
    bool hasOwnedTemporary = false;
};
}
