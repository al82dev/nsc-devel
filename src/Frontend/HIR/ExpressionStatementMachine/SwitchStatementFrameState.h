#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct SwitchStatementFrameState final
{
    const MethodSwitchBinding* binding = nullptr;
    std::unique_ptr<ProjectOopHirNode> node;
    std::unique_ptr<ProjectOopHirNode> sectionNode;
    std::unique_ptr<ProjectOopHirNode> sectionBody;
    const ast::AstNode* sectionSyntax = nullptr;
    std::size_t nextSyntaxChild = 0U;
    std::size_t nextBindingSection = 0U;
    std::size_t nextSectionChild = 0U;
};
}
