#pragma once

#include "nativesharp/AST/AstNode.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::frontend { class ProjectCompilation; }
namespace nativesharp::interop { struct LibraryMetadata; }

namespace nativesharp::interop::metadata_detail
{
// The caller supplies factory calls whose candidate C++ return types and
// provenance were independently resolved. This helper never infers a type from
// a member name, and it declines locals that escape or receive any value whose
// owner and reference do not match a resolved initializer candidate.
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

inline std::unordered_map<const ast::AstNode*, std::vector<TemplateFactoryIdentity>> resolveTemplateFactoryObservations(
    const std::vector<TemplateFactoryObservation>& observations)
{
    std::unordered_map<const ast::AstNode*, std::vector<const TemplateFactoryObservation*>> candidates;
    std::set<const ast::AstNode*> ambiguous;
    for (const auto& observation : observations)
    {
        if (!observation.initializer) continue;
        if (!observation.projectable || observation.ownerIdentityKey.empty() || observation.clangUsr.empty() ||
            observation.externalSymbol.empty() || observation.referenceIdentity.empty() || observation.ownerClangUsr.empty())
        { ambiguous.insert(observation.initializer); continue; }
        bool duplicate = false;
        for (const auto* existing : candidates[observation.initializer])
        {
            if (existing->clangUsr != observation.clangUsr && existing->externalSymbol != observation.externalSymbol) continue;
            if (existing->clangUsr != observation.clangUsr || existing->externalSymbol != observation.externalSymbol ||
                existing->ownerIdentityKey != observation.ownerIdentityKey ||
                existing->ownerClangUsr != observation.ownerClangUsr ||
                existing->referenceIdentity != observation.referenceIdentity)
                ambiguous.insert(observation.initializer);
            duplicate = true;
        }
        if (!duplicate) candidates[observation.initializer].push_back(&observation);
    }
    std::unordered_map<const ast::AstNode*, std::vector<TemplateFactoryIdentity>> result;
    for (const auto& [initializer, values] : candidates)
    {
        if (ambiguous.contains(initializer)) continue;
        auto& identities = result[initializer];
        for (const auto* value : values)
        {
            TemplateFactoryIdentity identity{value->ownerIdentityKey, value->ownerClangUsr, value->referenceIdentity};
            if (std::find(identities.begin(), identities.end(), identity) == identities.end())
                identities.push_back(std::move(identity));
        }
    }
    return result;
}

// Selects all verified alternatives of C++ factories, including differing
// arities and variadic signatures. Overload resolution remains with the consumer.
bool selectResolvedTemplateInstanceMethods(const frontend::ProjectCompilation& compilation,
    const LibraryMetadata& metadata, std::vector<TemplateInstanceMethodCandidate>& output, std::string& error);

// Resolves the verified identities of a value stored in a template receiver. A
// conditional expression qualifies only when both arms are resolved factory calls
// with the same identity set, because each arm may produce the receiver. Nested
// conditionals and any other arm remain unresolved, so no owner is inferred from
// a method name.
inline const std::vector<TemplateFactoryIdentity>* resolveTemplateReceiverValue(const ast::AstNode* value,
    const std::unordered_map<const ast::AstNode*, std::vector<TemplateFactoryIdentity>>& resolvedInitializers)
{
    if (!value) return nullptr;
    const auto direct = resolvedInitializers.find(value);
    if (direct != resolvedInitializers.end()) return &direct->second;
    if (value->kind() != ast::AstNodeKind::ConditionalExpression || !value->first() || !value->second() ||
        !value->third()) return nullptr;
    const auto whenTrue = resolvedInitializers.find(value->second());
    const auto whenFalse = resolvedInitializers.find(value->third());
    if (whenTrue == resolvedInitializers.end() || whenFalse == resolvedInitializers.end() ||
        whenTrue->second.empty() || whenTrue->second.size() != whenFalse->second.size()) return nullptr;
    for (const auto& identity : whenTrue->second)
        if (std::find(whenFalse->second.begin(), whenFalse->second.end(), identity) == whenFalse->second.end())
            return nullptr;
    return &whenTrue->second;
}

inline std::vector<TemplateInstanceMethodCandidate> selectTemplateInstanceMethods(
    const ast::AstNode* root, const std::unordered_map<const ast::AstNode*, std::vector<TemplateFactoryIdentity>>& resolvedInitializers)
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
    for (const auto& [factory, identities] : resolvedInitializers)
    {
        if (!factory || identities.empty() || std::any_of(identities.begin(), identities.end(), [](const auto& identity) {
            return identity.ownerIdentityKey.empty() || identity.ownerClangUsr.empty() || identity.referenceIdentity.empty();
        })) continue;
        const auto receiver = parents.find(factory);
        const auto* arrow = receiver == parents.end() ? nullptr : receiver->second;
        const auto memberParent = arrow ? parents.find(arrow) : parents.end();
        const auto* member = memberParent == parents.end() ? nullptr : memberParent->second;
        const auto callParent = member ? parents.find(member) : parents.end();
        const auto* call = callParent == parents.end() ? nullptr : callParent->second;
        if (!arrow || arrow->kind() != ast::AstNodeKind::UnaryExpression || arrow->auxiliaryText() != "arrow" ||
            arrow->first() != factory || !member || member->kind() != ast::AstNodeKind::MemberExpression ||
            member->first() != arrow || member->text().empty() || !call ||
            call->kind() != ast::AstNodeKind::CallExpression || call->first() != member) continue;
        for (const auto& identity : identities) selected.emplace(identity.ownerIdentityKey, member->text());
    }
    for (const auto& [name, group] : declarations)
    {
        if (name.empty() || group.size() != 1 ||
            group[0]->kind() != ast::AstNodeKind::VariableDeclarationStatement || !group[0]->first()) continue;
        const auto* resolved = resolveTemplateReceiverValue(group[0]->first(), resolvedInitializers);
        if (!resolved || resolved->empty() ||
            std::any_of(resolved->begin(), resolved->end(), [](const auto& identity) {
                return identity.ownerIdentityKey.empty() || identity.ownerClangUsr.empty() || identity.referenceIdentity.empty();
            })) continue;
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
                const auto* origin = resolveTemplateReceiverValue(arrow->second(), resolvedInitializers);
                if (enclosing != parents.end() && enclosing->second &&
                    enclosing->second->kind() == ast::AstNodeKind::ExpressionStatement &&
                    origin && !origin->empty() &&
                    std::all_of(origin->begin(), origin->end(), [&](const auto& identity) {
                        return std::find(resolved->begin(), resolved->end(), identity) != resolved->end();
                    })) continue;
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
        if (safe) for (const auto& identity : *resolved)
            for (const auto& method : methods) selected.emplace(identity.ownerIdentityKey, method);
    }
    std::vector<TemplateInstanceMethodCandidate> result;
    result.reserve(selected.size());
    for (const auto& [owner, method] : selected) result.push_back({owner, method});
    return result;
}
}
