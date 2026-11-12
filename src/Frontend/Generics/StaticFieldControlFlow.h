#pragma once

#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "StaticFieldTemplateRewriter.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace nativesharp::frontend::internal
{
// Collect only unlabeled jumps whose nearest target is this static iteration.
// A shared jump reached with conflicting targets fails closed.
inline bool collectStaticFieldJumps(ast::AstNode* body, std::vector<ast::AstNode*>& targets, std::string& error)
{
    struct Work { ast::AstNode* node; std::size_t loops; std::size_t switches; };
    std::vector<Work> work{{body, 0, 0}};
    std::unordered_map<ast::AstNode*, bool> jumpTargets;
    std::size_t visits = 0;
    while (!work.empty())
    {
        Work current = work.back();
        work.pop_back();
        ast::AstNode* node = current.node;
        if (node == nullptr) continue;
        if (++visits > 400000) { error = "static field jump analysis exceeds visit limit"; return false; }
        using K = ast::AstNodeKind;
        if (node->kind() == K::StaticFieldIterationStatement || node->kind() == K::LambdaExpression) continue;
        const bool loop = node->kind() == K::WhileStatement || node->kind() == K::DoWhileStatement ||
            node->kind() == K::ForStatement || node->kind() == K::ForeachStatement || node->kind() == K::LoopExpression;
        if (loop) ++current.loops;
        if (node->kind() == K::SwitchStatement) ++current.switches;
        if ((node->kind() == K::BreakStatement || node->kind() == K::ContinueStatement) && node->text().empty())
        {
            const bool targetsStatic = current.loops == 0 &&
                (node->kind() == K::ContinueStatement || current.switches == 0);
            const auto [found, inserted] = jumpTargets.emplace(node, targetsStatic);
            if (!inserted && found->second != targetsStatic)
            { error = "static field jump has ambiguous shared syntax"; return false; }
            if (targetsStatic && node->first() != nullptr)
            { error = "static field break cannot carry a value"; return false; }
            if (targetsStatic && inserted) targets.push_back(node);
        }
        work.push_back({node->first(), current.loops, current.switches});
        work.push_back({node->second(), current.loops, current.switches});
        work.push_back({node->third(), current.loops, current.switches});
        for (ast::AstNode* child : node->children()) work.push_back({child, current.loops, current.switches});
    }
    return true;
}

inline ast::AstNode* staticFieldBreak(ast::AstArena& arena, const SourceRange& range, const std::string& label)
{
    auto* zero = arena.make<ast::AstNode>(ast::AstNodeKind::IntegerLiteralExpression, range);
    zero->setTokenKind(lexer::TokenKind::IntegerLiteral);
    zero->setText("0");
    zero->setIntegerValue(0);
    auto* jump = arena.make<ast::AstNode>(ast::AstNodeKind::BreakStatement, range);
    jump->setText(label);
    jump->setFirst(zero);
    return jump;
}

inline ast::AstNode* staticFieldLoopLocal(ast::AstArena& arena, const SourceRange& range,
                                          const std::string& local, const std::string& label,
                                          ast::AstNode* body)
{
    body->addChild(staticFieldBreak(arena, range, label));
    auto* loop = arena.make<ast::AstNode>(ast::AstNodeKind::LoopExpression, range);
    loop->setText(label);
    loop->setFirst(body);
    auto* variable = arena.make<ast::AstNode>(ast::AstNodeKind::VariableDeclarationStatement, range);
    variable->setText(local);
    variable->setTypeSyntax(arena.make<ast::TypeSyntax>(range, lexer::TokenKind::KwInt, "int"));
    variable->setFirst(loop);
    return variable;
}

inline bool rewriteStaticFieldJumps(ast::AstArena& arena, ast::AstNode* body,
                                    const std::string& outerLabel, const std::string& innerLabel,
                                    std::string& error)
{
    std::vector<ast::AstNode*> targets;
    if (!collectStaticFieldJumps(body, targets, error)) return false;
    std::vector<ast::AstNode*> nodes;
    std::vector<StaticFieldRewriteEdge> edges;
    std::unordered_map<ast::AstNode*, std::vector<std::size_t>> incoming;
    if (!collectStaticFieldRewriteGraph(body, nodes, edges, incoming, error)) return false;
    std::unordered_map<ast::AstNode*, ast::AstNode*> replacements;
    for (ast::AstNode* target : targets)
        replacements.emplace(target, staticFieldBreak(arena, target->range(),
            target->kind() == ast::AstNodeKind::BreakStatement ? outerLabel : innerLabel));
    for (const StaticFieldRewriteEdge& edge : edges)
    {
        const auto found = replacements.find(edge.child);
        if (found == replacements.end()) continue;
        switch (edge.slot)
        {
        case 0: edge.parent->setFirst(found->second); break;
        case 1: edge.parent->setSecond(found->second); break;
        case 2: edge.parent->setThird(found->second); break;
        case 3: edge.parent->replaceChild(edge.index, found->second); break;
        }
    }
    return true;
}
}
