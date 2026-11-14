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
// a member name, and it deliberately declines locals that can be reassigned or
// escape through any expression other than a direct arrow-method receiver.
struct TemplateInstanceMethodCandidate
{
    std::string ownerIdentityKey;
    std::string methodName;
};

struct TemplateFactoryObservation
{
    const ast::AstNode* initializer = nullptr;
    std::string ownerIdentityKey;
    std::string clangUsr;
    std::string externalSymbol;
    std::string referenceIdentity;
    bool projectable = false;
};

inline std::unordered_map<const ast::AstNode*, std::string> resolveTemplateFactoryObservations(
    const std::vector<TemplateFactoryObservation>& observations)
{
    std::unordered_map<const ast::AstNode*, const TemplateFactoryObservation*> unique;
    std::set<const ast::AstNode*> ambiguous;
    for (const auto& observation : observations)
    {
        if (!observation.initializer) continue;
        if (!observation.projectable || observation.ownerIdentityKey.empty() || observation.clangUsr.empty() ||
            observation.externalSymbol.empty() || observation.referenceIdentity.empty())
        { ambiguous.insert(observation.initializer); continue; }
        const auto [position, inserted] = unique.emplace(observation.initializer, &observation);
        if (!inserted && (position->second->ownerIdentityKey != observation.ownerIdentityKey ||
            position->second->clangUsr != observation.clangUsr || position->second->externalSymbol != observation.externalSymbol ||
            position->second->referenceIdentity != observation.referenceIdentity))
            ambiguous.insert(observation.initializer);
    }
    std::unordered_map<const ast::AstNode*, std::string> result;
    for (const auto& [initializer, observation] : unique)
        if (!ambiguous.contains(initializer)) result.emplace(initializer, observation->ownerIdentityKey);
    return result;
}

// Selects only uniquely bound, zero-argument C++ factories with a verified
// source reference and imported record return. Output is unchanged on error.
bool selectResolvedTemplateInstanceMethods(const frontend::ProjectCompilation& compilation,
    const LibraryMetadata& metadata, std::vector<TemplateInstanceMethodCandidate>& output, std::string& error);

inline std::vector<TemplateInstanceMethodCandidate> selectTemplateInstanceMethods(
    const ast::AstNode* root, const std::unordered_map<const ast::AstNode*, std::string>& resolvedInitializers)
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
        if (resolved == resolvedInitializers.end() || resolved->second.empty()) continue;
        std::vector<std::string> methods;
        bool safe = true;
        for (const auto* use : uses[name])
        {
            const auto receiver = parents.find(use);
            const auto* arrow = receiver == parents.end() ? nullptr : receiver->second;
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
        if (safe) for (const auto& method : methods) selected.emplace(resolved->second, method);
    }
    std::vector<TemplateInstanceMethodCandidate> result;
    result.reserve(selected.size());
    for (const auto& [owner, method] : selected) result.push_back({owner, method});
    return result;
}
}
