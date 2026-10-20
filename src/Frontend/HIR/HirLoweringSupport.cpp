#include "HirLoweringSupport.h"

namespace nativesharp::frontend
{
std::unique_ptr<ProjectOopHirNode> makeNode(
    ProjectOopHirNodeKind kind,
    const ast::AstNode* syntax,
    std::string typeKey)
{
    auto node = std::make_unique<ProjectOopHirNode>();
    node->kind = kind;
    node->syntax = syntax;
    node->typeKey = std::move(typeKey);
    return node;
}

void destroyHirTreeIteratively(std::unique_ptr<ProjectOopHirNode>& root)
{
    root.reset();
}
}
