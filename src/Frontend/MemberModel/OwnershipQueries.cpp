#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "../Internal/EnumInitializerResolver.h"
#include "../Internal/AnonymousMemberPromotionVisitState.h"
#include "../Internal/EnumValue.h"
#include "../Internal/EnumSuccessor.h"

#include "nativesharp/Language/AggregateSemantics.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Compilation/ProjectGraph.h"

#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/PrecompiledMemberIdentityIndex.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <filesystem>
#include <functional>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
bool ProjectMemberModel::typeTransitivelyOwnsStorage(std::string_view typeKey) const
{
    if (nameLookup_ == nullptr) return false;

    std::string current(typeKey);
    while (const std::optional<std::string> element = fixedArrayElementTypeKey(current))
        current = *element;
    const std::optional<std::size_t> ordinal = typeOrdinalForKey(current);
    return ordinal.has_value() && *ordinal < transitiveOwnershipCache_.size() &&
        nameLookup_->namedTypes()[*ordinal].kind == NamedTypeDeclarationKind::Struct &&
        transitiveOwnershipCache_[*ordinal] != 0;
}

void ProjectMemberModel::buildTransitiveOwnershipCache()
{
    transitiveOwnershipCache_.assign(membersByType_.size(), 0);
    std::vector<std::vector<std::size_t>> dependents(membersByType_.size());
    std::deque<std::size_t> pending;

    for (std::size_t typeOrdinal = 0; typeOrdinal < membersByType_.size(); ++typeOrdinal)
    {
        if (nameLookup_ == nullptr || typeOrdinal >= nameLookup_->namedTypes().size() ||
            nameLookup_->namedTypes()[typeOrdinal].kind != NamedTypeDeclarationKind::Struct)
            continue;

        for (const std::size_t memberOrdinal : membersByType_[typeOrdinal])
        {
            if (memberOrdinal >= members_.size()) continue;
            const ClassMemberSymbol& member = members_[memberOrdinal];
            if (member.kind != ClassMemberKind::Field || member.isStatic) continue;
            if (member.declaredOwnership == sema::OwnershipKind::Owned)
            {
                if (transitiveOwnershipCache_[typeOrdinal] == 0)
                {
                    transitiveOwnershipCache_[typeOrdinal] = 1;
                    pending.push_back(typeOrdinal);
                }
                continue;
            }

            std::string nestedTypeKey(member.declaredTypeKey);
            while (const std::optional<std::string> element = fixedArrayElementTypeKey(nestedTypeKey))
                nestedTypeKey = *element;
            const std::optional<std::size_t> nestedOrdinal = typeOrdinalForKey(nestedTypeKey);
            if (!nestedOrdinal.has_value() || *nestedOrdinal >= membersByType_.size() ||
                nameLookup_->namedTypes()[*nestedOrdinal].kind != NamedTypeDeclarationKind::Struct)
                continue;
            dependents[*nestedOrdinal].push_back(typeOrdinal);
        }
    }

    while (!pending.empty())
    {
        const std::size_t owner = pending.front();
        pending.pop_front();
        for (const std::size_t dependent : dependents[owner])
        {
            if (transitiveOwnershipCache_[dependent] != 0) continue;
            transitiveOwnershipCache_[dependent] = 1;
            pending.push_back(dependent);
        }
    }
}

bool ProjectMemberModel::typeTransitivelyHasCustomDestructor(std::string_view typeKey) const
{
    if (nameLookup_ == nullptr) return false;

    std::vector<std::string> pending{std::string(typeKey)};
    std::unordered_set<std::size_t> visited;
    while (!pending.empty())
    {
        std::string candidate = std::move(pending.back());
        pending.pop_back();
        if (const std::optional<std::vector<std::string>> tuple =
                ProjectTypeConversion::tupleElementTypeKeys(candidate))
        {
            pending.insert(pending.end(), tuple->begin(), tuple->end());
            continue;
        }
        while (const std::optional<std::string> element = fixedArrayElementTypeKey(candidate))
            candidate = *element;
        const std::optional<std::size_t> ordinal = typeOrdinalForKey(candidate);
        if (!ordinal.has_value() ||
            nameLookup_->namedTypes()[*ordinal].kind != NamedTypeDeclarationKind::Struct)
            continue;
        if (!visited.insert(*ordinal).second) continue;
        for (const std::size_t memberOrdinal : membersByType_[*ordinal])
        {
            if (memberOrdinal >= members_.size()) continue;
            const ClassMemberSymbol& member = members_[memberOrdinal];
            if (member.kind == ClassMemberKind::Destructor) return true;
            if (member.kind == ClassMemberKind::Field && !member.isStatic)
                pending.push_back(member.declaredTypeKey);
        }
    }
    return false;
}

bool ProjectMemberModel::typeRequiresValueDrop(std::string_view typeKey) const
{
    // NS304-T06-F07: iterator/async-iterator carry a Drop function pointer and therefore always
    // require a value drop (the drop glue dispatches through it), like a custom-destructor struct.
    return typeTransitivelyOwnsStorage(typeKey) ||
        typeTransitivelyHasCustomDestructor(typeKey) ||
        typeKey.starts_with("iterator:") || typeKey.starts_with("async-iterator:");
}

bool ProjectMemberModel::typeIsImplicitlyCopyable(std::string_view typeKey) const
{
    std::vector<std::string> pending{std::string(typeKey)};
    std::unordered_set<std::size_t> visited;
    while (!pending.empty())
    {
        std::string candidate = std::move(pending.back());
        pending.pop_back();
        if (candidate.empty() || candidate == "<error>" || candidate == "builtin:void")
            return false;
        if (language::isFunctionTypeIdentity(candidate)) continue;
        if (candidate.starts_with("builtin:") || candidate.ends_with("*") ||
            candidate.ends_with("[]"))
            continue;
        if (const std::optional<std::vector<std::string>> tuple =
                ProjectTypeConversion::tupleElementTypeKeys(candidate))
        {
            pending.insert(pending.end(), tuple->begin(), tuple->end());
            continue;
        }
        if (const std::optional<std::string> element = fixedArrayElementTypeKey(candidate))
        {
            pending.push_back(*element);
            continue;
        }
        if (nameLookup_ == nullptr) return false;
        const std::optional<std::size_t> ordinal = typeOrdinalForKey(candidate);
        if (!ordinal.has_value()) return false;
        const NamedTypeDeclaration& declaration = nameLookup_->namedTypes()[*ordinal];
        if (declaration.kind != NamedTypeDeclarationKind::Struct) continue;

        const std::string_view markers = declaration.syntax != nullptr
            ? std::string_view(declaration.syntax->auxiliaryText()) : std::string_view{};
        const bool declaredMoveOnly = markers.find("move-only-value") != std::string_view::npos;
        const bool explicitCopy = markers.find("copy") != std::string_view::npos;
        if (declaredMoveOnly && !explicitCopy) return false;
        // Recursive by-value aggregates are rejected by layout. Returning true at the recursive
        // edge avoids manufacturing a second diagnostic while the outer fields are inspected.
        if (!visited.insert(*ordinal).second) continue;
        for (const std::size_t memberOrdinal : membersByType_[*ordinal])
        {
            if (memberOrdinal >= members_.size()) continue;
            const ClassMemberSymbol& field = members_[memberOrdinal];
            if (field.kind != ClassMemberKind::Field || field.isStatic || field.isConst) continue;
            if (field.declaredOwnership == sema::OwnershipKind::Owned) return false;
            pending.push_back(field.declaredTypeKey);
        }
    }
    return true;
}

bool ProjectMemberModel::typeIsMoveOnly(std::string_view typeKey) const
{
    std::vector<std::string> pending{std::string(typeKey)};
    while (!pending.empty())
    {
        std::string candidate = std::move(pending.back());
        pending.pop_back();
        if (candidate.empty() || candidate == "<error>" || candidate == "builtin:void" ||
            candidate.starts_with("builtin:") || candidate.ends_with("*") ||
            candidate.ends_with("[]"))
            continue;
        if (const std::optional<std::vector<std::string>> tuple =
                ProjectTypeConversion::tupleElementTypeKeys(candidate))
        {
            pending.insert(pending.end(), tuple->begin(), tuple->end());
            continue;
        }
        if (const std::optional<std::string> element = fixedArrayElementTypeKey(candidate))
        {
            pending.push_back(*element);
            continue;
        }
        const std::optional<std::size_t> ordinal = typeOrdinalForKey(candidate);
        if (ordinal.has_value() && nameLookup_ != nullptr &&
            nameLookup_->namedTypes()[*ordinal].kind == NamedTypeDeclarationKind::Struct &&
            !typeIsImplicitlyCopyable(candidate))
            return true;
    }
    return false;
}

std::span<const std::size_t> ProjectMemberModel::promotedMembersOfType(
    std::size_t typeOrdinal) const noexcept
{
    static const std::vector<std::size_t> empty;
    return typeOrdinal < promotedMembersByType_.size()
        ? std::span<const std::size_t>(promotedMembersByType_[typeOrdinal])
        : std::span<const std::size_t>(empty);
}

std::span<const std::size_t> ProjectMemberModel::findMembers(
    std::size_t typeOrdinal,
    std::string_view name) const noexcept
{
    static const std::vector<std::size_t> empty;
    if (typeOrdinal >= membersByName_.size()) return empty;
    const auto found = membersByName_[typeOrdinal].find(name);
    return found != membersByName_[typeOrdinal].end()
        ? std::span<const std::size_t>(found->second)
        : std::span<const std::size_t>(empty);
}

std::span<const std::size_t> ProjectMemberModel::findMembers(
    std::string_view name) const noexcept
{
    return indexes_->memberOrdinalsForName(name);
}

std::span<const std::size_t> ProjectMemberModel::findMembersBySignature(
    std::size_t typeOrdinal,
    std::string_view signature) const noexcept
{
    return indexes_->memberOrdinalsForSignature(typeOrdinal, signature);
}

std::optional<std::size_t> ProjectMemberModel::typeOrdinalForKey(
    std::string_view typeKey) const noexcept
{
    if (!typeKey.starts_with("type:")) return std::nullopt;
    const auto found = typeOrdinalByKey_.find(typeKey.substr(5));
    return found != typeOrdinalByKey_.end()
        ? std::optional<std::size_t>(found->second)
        : std::nullopt;
}

void ProjectMemberModel::buildMemberNameIndex()
{
    membersByName_.clear();
    membersByName_.resize(membersByType_.size());
    for (std::size_t typeOrdinal = 0; typeOrdinal < membersByType_.size(); ++typeOrdinal)
    {
        MemberNameIndex& byName = membersByName_[typeOrdinal];
        for (const std::size_t memberOrdinal : membersByType_[typeOrdinal])
        {
            if (memberOrdinal >= members_.size()) continue;
            const ClassMemberSymbol& member = members_[memberOrdinal];
            if (member.isCompilerGenerated && member.syntax != nullptr &&
                member.syntax->auxiliaryText().find("anonymous-member-carrier") != std::string::npos)
                continue;
            byName[member.name].push_back(memberOrdinal);
        }
        if (typeOrdinal >= promotedMembersByType_.size()) continue;
        for (const std::size_t memberOrdinal : promotedMembersByType_[typeOrdinal])
        {
            if (memberOrdinal < members_.size())
                byName[members_[memberOrdinal].name].push_back(memberOrdinal);
        }
    }
}

}
