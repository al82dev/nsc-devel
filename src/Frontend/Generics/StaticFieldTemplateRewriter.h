#pragma once

#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "StaticFieldRewriteEdge.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::frontend::internal
{
// This helper rewrites only a private, already-cloned body. The caller must
// validate every TypeSyntax separately, bind each returned access to the exact
// field identity, and verify receiver/accessibility before publishing the body.
inline bool staticFieldDescriptorReceiver(const ast::AstNode* member, std::string_view descriptor)
{
    return member != nullptr && member->kind() == ast::AstNodeKind::MemberExpression &&
        member->first() != nullptr && member->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
        member->first()->text() == descriptor;
}

// nameof is lexical and does not evaluate a call's arguments. Only known
// descriptor forms are rewritten; all other uses remain subject to validation.
inline bool staticFieldNameOf(const ast::AstNode* node, std::string_view descriptor)
{
    using K = ast::AstNodeKind;
    if (node == nullptr || node->kind() != K::NameOfExpression || node->first() == nullptr ||
        !node->typeSyntaxes().empty()) return false;
    const ast::AstNode* target = node->first();
    bool call = false;
    if (target->kind() == K::CallExpression)
    {
        if (!target->typeSyntaxes().empty()) return false;
        target = target->first();
        call = true;
    }
    if (target == nullptr) return false;
    if (!call && target->kind() == K::IdentifierExpression && target->text() == descriptor)
        return node->text() == descriptor;
    if (!staticFieldDescriptorReceiver(target, descriptor)) return false;
    if (target->text() == "Name") return !call && node->text() == "Name";
    return target->text() == "Get" && node->text() == "Get";
}

// Reject declaration shadows until the expander carries lexical descriptor identities.
inline bool staticFieldShadowsDescriptor(const ast::AstNode* node, std::string_view descriptor)
{
    using ast::AstNodeKind;
    for (const std::string& name : node->genericParameters())
        if (name == descriptor) return true;
    if (node->text() != descriptor) return false;
    switch (node->kind())
    {
    case AstNodeKind::FunctionDeclaration:
    case AstNodeKind::MethodDeclaration:
    case AstNodeKind::ClassDeclaration:
    case AstNodeKind::StructDeclaration:
    case AstNodeKind::InterfaceDeclaration:
    case AstNodeKind::DelegateDeclaration:
    case AstNodeKind::UsingAliasDirective:
    case AstNodeKind::VariableDeclarationStatement:
    case AstNodeKind::ParameterDeclaration:
    case AstNodeKind::ForeachStatement:
    case AstNodeKind::CatchClause:
    case AstNodeKind::DeconstructionDeclarationStatement:
        return true;
    default:
        return false;
    }
}

// Validation and node allocation complete before any edge or output vector is
// changed. The explicit graph also handles first/children aliases without
// duplicating a replacement or evaluating a Get receiver more than once.
inline bool collectStaticFieldRewriteGraph(
    ast::AstNode* clonedBody, std::vector<ast::AstNode*>& nodes,
    std::vector<StaticFieldRewriteEdge>& edges,
    std::unordered_map<ast::AstNode*, std::vector<std::size_t>>& incoming, std::string& error,
    std::string_view shadowedDescriptor = {})
{
    using ast::AstNode;
    constexpr std::size_t maximumNodes = 100000;
    std::vector<AstNode*> pending{clonedBody};
    std::unordered_set<AstNode*> visited;
    while (!pending.empty())
    {
        AstNode* node = pending.back();
        pending.pop_back();
        if (!visited.insert(node).second) continue;
        if (nodes.size() == maximumNodes)
        {
            error = "static field rewrite exceeds node limit";
            return false;
        }
        nodes.push_back(node);
        if (!shadowedDescriptor.empty() && staticFieldNameOf(node, shadowedDescriptor)) continue;
        const std::size_t maximumEdges = maximumNodes * 4;
        if (edges.size() > maximumEdges - 3 ||
            node->children().size() > maximumEdges - 3 - edges.size())
        {
            error = "static field rewrite exceeds edge limit";
            return false;
        }
        const auto add = [&](AstNode* child, std::size_t slot, std::size_t index)
        {
            if (child == nullptr) return;
            incoming[child].push_back(edges.size());
            edges.push_back({node, child, slot, index});
            pending.push_back(child);
        };
        const AstNode* hiddenBody = !shadowedDescriptor.empty() &&
            node->kind() == ast::AstNodeKind::StaticFieldIterationStatement &&
            node->text() == shadowedDescriptor ? node->first() : nullptr;
        if (node->first() != hiddenBody) add(node->first(), 0, 0);
        if (node->second() != hiddenBody) add(node->second(), 1, 0);
        if (node->third() != hiddenBody) add(node->third(), 2, 0);
        for (std::size_t index = 0; index < node->children().size(); ++index)
            if (node->children()[index] != hiddenBody) add(node->children()[index], 3, index);
    }
    return true;
}

inline bool staticFieldRewriteGraphIsAcyclic(const std::vector<ast::AstNode*>& nodes,
    const std::vector<StaticFieldRewriteEdge>& edges, std::string& error)
{
    using ast::AstNode;
    std::unordered_map<AstNode*, std::size_t> incomingCount;
    for (AstNode* node : nodes) incomingCount.emplace(node, 0);
    for (const StaticFieldRewriteEdge& edge : edges) ++incomingCount.at(edge.child);
    std::vector<AstNode*> ready;
    for (AstNode* node : nodes) if (incomingCount.at(node) == 0) ready.push_back(node);
    std::unordered_map<AstNode*, std::vector<AstNode*>> outgoing;
    for (const StaticFieldRewriteEdge& edge : edges) outgoing[edge.parent].push_back(edge.child);
    std::size_t consumed = 0;
    while (!ready.empty())
    {
        AstNode* node = ready.back();
        ready.pop_back();
        ++consumed;
        for (AstNode* child : outgoing[node]) if (--incomingCount.at(child) == 0) ready.push_back(child);
    }
    if (consumed != nodes.size())
    {
        error = "cyclic syntax graph in static field body";
        return false;
    }
    return true;
}

// nameof caches text at parse time. Replacing a descendant without rebuilding
// that cache could silently name a different expression, so reject this form.
inline bool staticFieldDescriptorIsInsideNameOf(
    const std::vector<ast::AstNode*>& nodes, const std::vector<StaticFieldRewriteEdge>& edges,
    std::unordered_map<ast::AstNode*, std::vector<std::size_t>>& incoming, std::string_view descriptor)
{
    std::vector<ast::AstNode*> pending;
    std::unordered_set<ast::AstNode*> visited;
    for (ast::AstNode* node : nodes)
        if (node->kind() == ast::AstNodeKind::IdentifierExpression && node->text() == descriptor)
            pending.push_back(node);
    while (!pending.empty())
    {
        ast::AstNode* node = pending.back();
        pending.pop_back();
        if (!visited.insert(node).second) continue;
        for (std::size_t index : incoming[node])
        {
            ast::AstNode* parent = edges[index].parent;
            if (parent->kind() == ast::AstNodeKind::NameOfExpression) return true;
            pending.push_back(parent);
        }
    }
    return false;
}

// Control flow in a lambda has its own boundary. Until targets are modeled
// across that boundary, reject its jumps rather than inheriting outer loops.
inline bool staticFieldLambdaContainsJump(
    const std::vector<ast::AstNode*>& nodes, const std::vector<StaticFieldRewriteEdge>& edges,
    std::unordered_map<ast::AstNode*, std::vector<std::size_t>>& incoming)
{
    std::vector<ast::AstNode*> pending;
    std::unordered_set<ast::AstNode*> visited;
    for (ast::AstNode* node : nodes)
        if (node->kind() == ast::AstNodeKind::BreakStatement || node->kind() == ast::AstNodeKind::ContinueStatement)
            pending.push_back(node);
    while (!pending.empty())
    {
        ast::AstNode* node = pending.back();
        pending.pop_back();
        if (!visited.insert(node).second) continue;
        for (std::size_t index : incoming[node])
        {
            ast::AstNode* parent = edges[index].parent;
            if (parent->kind() == ast::AstNodeKind::LambdaExpression) return true;
            pending.push_back(parent);
        }
    }
    return false;
}

inline bool isStaticFieldInnerLoop(ast::AstNodeKind kind) noexcept
{
    using ast::AstNodeKind;
    return kind == AstNodeKind::WhileStatement || kind == AstNodeKind::DoWhileStatement ||
        kind == AstNodeKind::ForStatement || kind == AstNodeKind::ForeachStatement ||
        kind == AstNodeKind::LoopExpression;
}

// A path-sensitive walk is required because a shared AST node can occur both
// inside and outside a nested loop. Labels may target only loops in this body.
inline bool validateStaticFieldControlFlow(ast::AstNode* body, std::string& error,
    const std::vector<const ast::AstNode*>& outerLabels = {}, bool allowOuterBreakValue = true)
{
    using ast::AstNode;
    using ast::AstNodeKind;
    constexpr std::size_t maximumVisits = 400000;
    std::vector<std::pair<AstNode*, bool>> pending{{body, false}};
    std::unordered_map<std::string, std::size_t> labels;
    std::size_t loops = 0;
    std::size_t switches = 0;
    std::size_t visits = 0;
    while (!pending.empty())
    {
        const auto [node, leaving] = pending.back();
        pending.pop_back();
        const bool loop = isStaticFieldInnerLoop(node->kind());
        const bool switchNode = node->kind() == AstNodeKind::SwitchStatement;
        const bool labeledLoop = node->kind() == AstNodeKind::LoopExpression && !node->text().empty();
        if (leaving)
        {
            if (labeledLoop && --labels.at(node->text()) == 0) labels.erase(node->text());
            if (loop) --loops;
            if (switchNode) --switches;
            continue;
        }
        if (++visits > maximumVisits)
        {
            error = "static field control-flow validation exceeds visit limit";
            return false;
        }
        if (loop) ++loops;
        if (switchNode) ++switches;
        if (labeledLoop) ++labels[node->text()];
        if (node->kind() == AstNodeKind::BreakStatement || node->kind() == AstNodeKind::ContinueStatement)
        {
            const bool named = !node->text().empty();
            std::size_t outerMatches = 0;
            if (named && !labels.contains(node->text()))
                for (const AstNode* outer : outerLabels)
                    if (outer != nullptr && outer->kind() == AstNodeKind::LoopExpression &&
                        outer->text() == node->text()) ++outerMatches;
            const bool permitted = named ? (labels.contains(node->text()) || outerMatches == 1) :
                (loops != 0 || (node->kind() == AstNodeKind::BreakStatement && switches != 0));
            if (named && !permitted)
            {
                error = "break or continue cannot target a static field iteration or an outer scope";
                return false;
            }
            if (named && outerMatches == 1 && node->kind() == AstNodeKind::BreakStatement &&
                node->first() != nullptr && !allowOuterBreakValue)
            {
                error = "static field iteration cannot verify an outer break value in an empty selection";
                return false;
            }
        }
        pending.emplace_back(node, true);
        if (node->first() != nullptr) pending.emplace_back(node->first(), false);
        if (node->second() != nullptr) pending.emplace_back(node->second(), false);
        if (node->third() != nullptr) pending.emplace_back(node->third(), false);
        for (AstNode* child : node->children()) if (child != nullptr) pending.emplace_back(child, false);
    }
    return true;
}

inline bool validateStaticFieldRewrite(
    const std::vector<ast::AstNode*>& nodes, const std::vector<StaticFieldRewriteEdge>& edges,
    std::unordered_map<ast::AstNode*, std::vector<std::size_t>>& incoming, std::string_view descriptor,
    std::vector<ast::AstNode*>& names, std::vector<ast::AstNode*>& gets, std::string& error,
    const std::vector<const ast::AstNode*>& outerLabels = {}, bool allowOuterBreakValue = true)
{
    using ast::AstNode;
    using ast::AstNodeKind;
    if (nodes.empty() || !validateStaticFieldControlFlow(nodes.front(), error, outerLabels, allowOuterBreakValue)) return false;
    if (staticFieldDescriptorIsInsideNameOf(nodes, edges, incoming, descriptor))
    {
        error = "static field descriptor inside nameof is not supported";
        return false;
    }
    if (staticFieldLambdaContainsJump(nodes, edges, incoming))
    {
        error = "break or continue inside a static field lambda is not supported";
        return false;
    }
    for (AstNode* node : nodes)
    {
        if (staticFieldShadowsDescriptor(node, descriptor))
        {
            error = "nested or shadowed static field descriptor is not supported";
            return false;
        }
        if (node->kind() == AstNodeKind::IdentifierExpression && node->text() == descriptor)
        {
            for (std::size_t index : incoming[node])
            {
                const StaticFieldRewriteEdge& edge = edges[index];
                if (edge.slot != 0 ||
                    !staticFieldDescriptorReceiver(edge.parent, descriptor) ||
                    (edge.parent->text() != "Name" && edge.parent->text() != "Get"))
                {
                    error = "unsupported static field descriptor expression";
                    return false;
                }
            }
        }
        if (!staticFieldDescriptorReceiver(node, descriptor)) continue;
        if (node->text() == "Name")
        {
            names.push_back(node);
            continue;
        }
        if (node->text() != "Get" || incoming[node].empty())
        {
            error = "unsupported static field descriptor member";
            return false;
        }
        for (std::size_t index : incoming[node])
        {
            const StaticFieldRewriteEdge& edge = edges[index];
            AstNode* call = edge.parent;
            if (edge.slot != 0 || call->kind() != AstNodeKind::CallExpression ||
                call->children().size() != 1 || call->children()[0] == nullptr || call->flag() ||
                !call->typeSyntaxes().empty() || call->second() != nullptr || call->third() != nullptr ||
                !call->children()[0]->argumentName().empty())
            {
                error = "static field Get requires exactly one positional receiver";
                return false;
            }
            gets.push_back(call);
        }
    }
    return true;
}

inline void applyStaticFieldRewrite(
    ast::AstArena& arena, const std::vector<StaticFieldRewriteEdge>& edges,
    const std::vector<ast::AstNode*>& nodes, const std::vector<ast::AstNode*>& names,
    const std::vector<ast::AstNode*>& gets, std::string_view descriptor,
    std::string_view fieldName, std::vector<const ast::AstNode*>& accesses)
{
    using ast::AstNode;
    using ast::AstNodeKind;
    std::unordered_map<AstNode*, AstNode*> replacement;
    std::vector<const AstNode*> newAccesses;
    for (AstNode* node : nodes)
    {
        if (!staticFieldNameOf(node, descriptor)) continue;
        AstNode* literal = arena.make<AstNode>(AstNodeKind::StringLiteralExpression, node->range());
        literal->setTokenKind(lexer::TokenKind::StringLiteral);
        literal->setText(node->text());
        literal->setArgumentName(node->argumentName());
        replacement.emplace(node, literal);
    }
    for (AstNode* node : names)
    {
        AstNode* literal = arena.make<AstNode>(AstNodeKind::StringLiteralExpression, node->range());
        literal->setTokenKind(lexer::TokenKind::StringLiteral);
        literal->setText(std::string(fieldName));
        literal->setArgumentName(node->argumentName());
        replacement.emplace(node, literal);
    }
    for (AstNode* call : gets)
    {
        if (replacement.contains(call)) continue;
        AstNode* member = arena.make<AstNode>(AstNodeKind::MemberExpression, call->range());
        member->setText(std::string(fieldName));
        member->setArgumentName(call->argumentName());
        replacement.emplace(call, member);
        newAccesses.push_back(member);
    }
    for (AstNode* call : gets)
    {
        AstNode* receiver = call->children()[0];
        const auto rewritten = replacement.find(receiver);
        replacement.at(call)->setFirst(rewritten == replacement.end() ? receiver : rewritten->second);
    }
    accesses.reserve(accesses.size() + newAccesses.size());
    for (const StaticFieldRewriteEdge& edge : edges)
    {
        const auto found = replacement.find(edge.child);
        if (found == replacement.end()) continue;
        switch (edge.slot)
        {
        case 0: edge.parent->setFirst(found->second); break;
        case 1: edge.parent->setSecond(found->second); break;
        case 2: edge.parent->setThird(found->second); break;
        case 3: edge.parent->replaceChild(edge.index, found->second); break;
        }
    }
    accesses.insert(accesses.end(), newAccesses.begin(), newAccesses.end());
}

inline bool rewriteStaticFieldTemplate(
    ast::AstArena& arena, ast::AstNode* clonedBody, std::string_view descriptor,
    std::string_view fieldName, std::vector<const ast::AstNode*>& accesses, std::string& error,
    const std::vector<const ast::AstNode*>& outerLabels = {})
{
    if (clonedBody == nullptr || clonedBody->kind() != ast::AstNodeKind::BlockStatement ||
        descriptor.empty() || fieldName.empty())
    {
        error = "invalid static field rewrite input";
        return false;
    }
    std::vector<ast::AstNode*> nodes;
    std::vector<StaticFieldRewriteEdge> edges;
    std::unordered_map<ast::AstNode*, std::vector<std::size_t>> incoming;
    std::vector<ast::AstNode*> names;
    std::vector<ast::AstNode*> gets;
    if (!collectStaticFieldRewriteGraph(clonedBody, nodes, edges, incoming, error, descriptor) ||
        !staticFieldRewriteGraphIsAcyclic(nodes, edges, error) ||
        !validateStaticFieldRewrite(nodes, edges, incoming, descriptor, names, gets, error, outerLabels)) return false;
    applyStaticFieldRewrite(arena, edges, nodes, names, gets, descriptor, fieldName, accesses);
    error.clear();
    return true;
}
}
