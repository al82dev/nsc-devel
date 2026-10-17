#include "HirLoweringSupport.h"

#include <utility>

namespace nativesharp::frontend
{
void ProjectOopHirModel::applyStructuredExitBinding(
    ProjectOopHirNode& node,
    const ast::AstNode* syntax) const
{
    const MethodStructuredExitBinding* binding = structuredExitBinding(syntax);
    if (binding == nullptr) return;
    node.cleanupRegionOrdinals = binding->cleanupRegionOrdinals;
    node.targetScopeOrdinal = binding->targetScopeOrdinal;
    switch (binding->kind)
    {
        case MethodStructuredExitKind::Return:
            node.cleanupContinuationKind = MethodCleanupContinuationKind::Return;
            break;
        case MethodStructuredExitKind::Break:
            node.cleanupContinuationKind = MethodCleanupContinuationKind::Break;
            break;
        case MethodStructuredExitKind::Continue:
            node.cleanupContinuationKind = MethodCleanupContinuationKind::Continue;
            break;
    }
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::wrapFullExpressionCleanup(
    std::unique_ptr<ProjectOopHirNode> value,
    const ast::AstNode* syntax,
    bool hasOwnedTemporary)
{
    if (!hasOwnedTemporary) return value;
    auto fullExpression = makeNode(ProjectOopHirNodeKind::FullExpressionCleanup, syntax, value->typeKey);
    fullExpression->children.push_back(std::move(value));
    return fullExpression;
}
}
