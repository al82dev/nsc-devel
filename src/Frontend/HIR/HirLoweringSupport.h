#pragma once

#include "nativesharp/Frontend/ProjectOopHirModel.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

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

inline void destroyHirTreeIteratively(std::unique_ptr<ProjectOopHirNode>& root)
{
    if (root == nullptr) return;
    std::vector<std::unique_ptr<ProjectOopHirNode>> pending;
    pending.reserve(1U);
    pending.push_back(std::move(root));
    while (!pending.empty())
    {
        std::unique_ptr<ProjectOopHirNode> current = std::move(pending.back());
        pending.pop_back();
        std::size_t childCount = 0;
        for (const std::unique_ptr<ProjectOopHirNode>& child : current->children)
            if (child != nullptr) ++childCount;
        pending.reserve(pending.size() + childCount);
        for (std::unique_ptr<ProjectOopHirNode>& child : current->children)
            if (child != nullptr) pending.push_back(std::move(child));
        current->children.clear();
    }
}
}
