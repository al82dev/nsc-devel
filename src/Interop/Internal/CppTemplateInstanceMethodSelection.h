#pragma once

#include "nativesharp/AST/AstNode.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace nativesharp::frontend { class ProjectCompilation; }
namespace nativesharp::interop { struct LibraryMetadata; }

namespace nativesharp::interop::metadata_detail
{
// The caller must supply only initializer calls whose exact C++ return type and
// provenance were independently resolved. This helper never infers a type from
// a member name, and it declines locals that escape or receive any value whose
// owner and reference do not match the resolved initializer.
struct TemplateInstanceMethodCandidate
{
    std::string ownerIdentityKey;
    std::string methodName;
};

struct TemplateFactoryIdentity
{
    std::string ownerIdentityKey;
    std::string ownerClangUsr;
    std::string referenceIdentity;
    bool operator==(const TemplateFactoryIdentity&) const = default;
};

struct TemplateFactoryObservation
{
    const ast::AstNode* initializer = nullptr;
    std::string ownerIdentityKey;
    std::string clangUsr;
    std::string externalSymbol;
    std::string referenceIdentity;
    std::string ownerClangUsr;
    bool projectable = false;
};

inline std::unordered_map<const ast::AstNode*, TemplateFactoryIdentity> resolveTemplateFactoryObservations(
    const std::vector<TemplateFactoryObservation>& observations)
{
    std::unordered_map<const ast::AstNode*, const TemplateFactoryObservation*> unique;
    std::set<const ast::AstNode*> ambiguous;
    for (const auto& observation : observations)
    {
        if (!observation.initializer) continue;
        if (!observation.projectable || observation.ownerIdentityKey.empty() || observation.clangUsr.empty() ||
            observation.externalSymbol.empty() || observation.referenceIdentity.empty() || observation.ownerClangUsr.empty())
        { ambiguous.insert(observation.initializer); continue; }
        const auto [position, inserted] = unique.emplace(observation.initializer, &observation);
        if (!inserted && (position->second->ownerIdentityKey != observation.ownerIdentityKey ||
            position->second->clangUsr != observation.clangUsr || position->second->externalSymbol != observation.externalSymbol ||
            position->second->referenceIdentity != observation.referenceIdentity ||
            position->second->ownerClangUsr != observation.ownerClangUsr))
            ambiguous.insert(observation.initializer);
    }
    std::unordered_map<const ast::AstNode*, TemplateFactoryIdentity> result;
    for (const auto& [initializer, observation] : unique)
        if (!ambiguous.contains(initializer)) result.emplace(initializer, TemplateFactoryIdentity{
            observation->ownerIdentityKey, observation->ownerClangUsr, observation->referenceIdentity});
    return result;
}

// Selects only uniquely bound, nonvariadic C++ factories with matching argument
// count, a verified source reference and imported record return. Output is unchanged on error.
bool selectResolvedTemplateInstanceMethods(const frontend::ProjectCompilation& compilation,
    const LibraryMetadata& metadata, std::vector<TemplateInstanceMethodCandidate>& output, std::string& error);

inline std::vector<TemplateInstanceMethodCandidate> selectTemplateInstanceMethods(
    const ast::AstNode* root, const std::unordered_map<const ast::AstNode*, TemplateFactoryIdentity>& resolvedInitializers)
{
    if (!root) return {};
    std::unordered_map<const ast::AstNode*, const ast::AstNode*> parents;
    std::vector<const ast::AstNode*> pending{root};
    parents.emplace(root, nullptr);
    std::map<std::string, std::vector<const ast::AstNode*>> declarations;
    std::map<std::string, std::vector<const ast::AstNode*>> uses;
    bool conflictingParents = false;
    while (!pending.empty())
    {
        const auto* node = pending.back();
        pending.pop_back();
        if (node->kind() == ast::AstNodeKind::VariableDeclarationStatement ||
            node->kind() == ast::AstNodeKind::ParameterDeclaration)
            declarations[node->text()].push_back(node);
        if (node->kind() == ast::AstNodeKind::IdentifierExpression)
            uses[node->text()].push_back(node);
        const auto add = [&](const ast::AstNode* child) {
            if (!child) return;
            const auto [position, inserted] = parents.emplace(child, node);
            if (inserted) pending.push_back(child);
            else if (position->second != node) conflictingParents = true;
        };
        add(node->first()); add(node->second()); add(node->third());
        for (const auto* child : node->children()) add(child);
    }
    if (conflictingParents) return {};
    std::set<std::pair<std::string, std::string>> selected;
    for (const auto& [name, group] : declarations)
    {
        if (name.empty() || group.size() != 1 ||
            group[0]->kind() != ast::AstNodeKind::VariableDeclarationStatement || !group[0]->first()) continue;
        const auto resolved = resolvedInitializers.find(group[0]->first());
        if (resolved == resolvedInitializers.end() || resolved->second.ownerIdentityKey.empty() ||
            resolved->second.ownerClangUsr.empty() || resolved->second.referenceIdentity.empty()) continue;
        std::vector<std::string> methods;
        bool safe = true;
        for (const auto* use : uses[name])
        {
            const auto receiver = parents.find(use);
            const auto* arrow = receiver == parents.end() ? nullptr : receiver->second;
            if (arrow && arrow->kind() == ast::AstNodeKind::AssignmentExpression && arrow->first() == use &&
                arrow->tokenKind() == lexer::TokenKind::Equal && !arrow->isReference() && arrow->second())
            {
                const auto enclosing = parents.find(arrow);
                const auto origin = resolvedInitializers.find(arrow->second());
                if (enclosing != parents.end() && enclosing->second &&
                    enclosing->second->kind() == ast::AstNodeKind::ExpressionStatement &&
                    origin != resolvedInitializers.end() && origin->second == resolved->second) continue;
                safe = false; break;
            }
            const auto memberParent = arrow ? parents.find(arrow) : parents.end();
            const auto* member = memberParent == parents.end() ? nullptr : memberParent->second;
            const auto callParent = member ? parents.find(member) : parents.end();
            const auto* call = callParent == parents.end() ? nullptr : callParent->second;
            if (!arrow || arrow->kind() != ast::AstNodeKind::UnaryExpression ||
                arrow->auxiliaryText() != "arrow" || arrow->first() != use ||
                !member || member->kind() != ast::AstNodeKind::MemberExpression || member->first() != arrow ||
                member->text().empty() || !call || call->kind() != ast::AstNodeKind::CallExpression ||
                call->first() != member)
            { safe = false; break; }
            methods.push_back(member->text());
        }
        if (safe) for (const auto& method : methods) selected.emplace(resolved->second.ownerIdentityKey, method);
    }
    std::vector<TemplateInstanceMethodCandidate> result;
    result.reserve(selected.size());
    for (const auto& [owner, method] : selected) result.push_back({owner, method});
    return result;
}
}
