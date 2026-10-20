#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct ReceiverFrameState final
{
    const ast::AstNode* syntax = nullptr;
    std::string_view typeKey;
    std::unique_ptr<ProjectOopHirNode> lowered;
    bool materializeTemporary = false;
};
}
