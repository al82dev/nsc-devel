#pragma once

#include "nativesharp/Frontend/ProjectOopHirModel.h"

#include <memory>
#include <string>
#include <utility>

namespace nativesharp::frontend
{
inline std::unique_ptr<ProjectOopHirNode> makeNode(
    ProjectOopHirNodeKind kind,
    const ast::AstNode* syntax,
    std::string typeKey = {})
{
    auto node = std::make_unique<ProjectOopHirNode>();
    node->kind = kind;
    node->syntax = syntax;
    node->typeKey = std::move(typeKey);
    return node;
}
}
