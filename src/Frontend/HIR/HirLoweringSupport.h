#pragma once

#include "HirAtomicCallPlan.h"
#include "HirCoreOperandCallPlan.h"
#include "HirMonitorCallPlan.h"
#include "HirTuplePatternFrame.h"

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
std::optional<HirCoreOperandCallPlan> hirCoreOperandCallPlan(
    MethodCoreOperationKind kind);

std::optional<HirAtomicCallPlan> hirAtomicCallPlan(
    MethodCoreOperationKind kind);

std::optional<HirMonitorCallPlan> hirMonitorCallPlan(
    MethodCoreOperationKind kind);

std::unique_ptr<ProjectOopHirNode> makeNode(
    ProjectOopHirNodeKind kind,
    const ast::AstNode* syntax,
    std::string typeKey = {});

void destroyHirTreeIteratively(std::unique_ptr<ProjectOopHirNode>& root);

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
