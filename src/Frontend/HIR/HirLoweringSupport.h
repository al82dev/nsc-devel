#pragma once

#include "nativesharp/Frontend/ProjectOopHirModel.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
struct HirCoreOperandCallPlan final
{
    std::size_t operandCount;
    ProjectOopHirNodeKind hirKind;
};

std::optional<HirCoreOperandCallPlan> hirCoreOperandCallPlan(
    MethodCoreOperationKind kind);

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
    root.reset();
}

struct HirTuplePatternFrame final
{
    const ast::AstNode* syntax = nullptr;
    std::string typeKey;
    std::uint64_t byteOffset = 0;
};

template<typename ExpandTuple, typename VisitLeaf>
bool visitHirTuplePatternLeaves(
    const ast::AstNode* syntax,
    std::string typeKey,
    std::uint64_t byteOffset,
    ExpandTuple&& expandTuple,
    VisitLeaf&& visitLeaf)
{
    std::vector<HirTuplePatternFrame> pending;
    pending.push_back(HirTuplePatternFrame{syntax, std::move(typeKey), byteOffset});
    while (!pending.empty())
    {
        HirTuplePatternFrame current = std::move(pending.back());
        pending.pop_back();
        if (current.syntax == nullptr) return false;
        if (current.syntax->kind() != ast::AstNodeKind::TupleExpression)
        {
            if (!visitLeaf(current.syntax, current.typeKey, current.byteOffset)) return false;
            continue;
        }

        std::vector<HirTuplePatternFrame> children;
        if (!expandTuple(
                current.syntax, current.typeKey, current.byteOffset, children))
        {
            return false;
        }
        pending.reserve(pending.size() + children.size());
        for (auto child = children.rbegin(); child != children.rend(); ++child)
            pending.push_back(std::move(*child));
    }
    return true;
}
}
