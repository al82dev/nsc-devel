#include "nativesharp/Frontend/ProjectCompilationIndexes.h"

#include "nativesharp/Frontend/ClassMemberSymbol.h"
#include "nativesharp/Frontend/ClassVirtualTable.h"
#include "nativesharp/Frontend/NamedTypeDeclaration.h"
#include "nativesharp/Frontend/VirtualMethodInfo.h"

#include <algorithm>

namespace nativesharp::frontend
{
void ProjectCompilationIndexes::clear() noexcept
{
    typesByOrdinal_.clear();
    typeOrdinalByIdentity_.clear();
    typeOrdinalsByName_.clear();
    membersByOrdinal_.clear();
    memberOrdinalsBySignature_.clear();
    virtualMethodsByMember_.clear();
    virtualTablesByType_.clear();
}

void ProjectCompilationIndexes::rebuildTypes(std::span<const NamedTypeDeclaration> types)
{
    clear();
    typesByOrdinal_.reserve(types.size());
    typeOrdinalByIdentity_.reserve(types.size());
    for (std::size_t ordinal = 0; ordinal < types.size(); ++ordinal)
    {
        const NamedTypeDeclaration& type = types[ordinal];
        typesByOrdinal_.push_back(&type);
        typeOrdinalByIdentity_.try_emplace(type.identity.key(), ordinal);
        typeOrdinalsByName_[type.identity.name()].push_back(ordinal);
        const std::string qualifiedName = type.identity.fullyQualifiedName();
        if (qualifiedName != type.identity.name())
        {
            typeOrdinalsByName_[qualifiedName].push_back(ordinal);
        }
    }
}

void ProjectCompilationIndexes::rebuildMembers(std::span<const ClassMemberSymbol> members)
{
    membersByOrdinal_.clear();
    memberOrdinalsBySignature_.clear();
    virtualMethodsByMember_.clear();
    virtualTablesByType_.clear();
    membersByOrdinal_.reserve(members.size());
    std::size_t typeCount = typesByOrdinal_.size();
    for (const ClassMemberSymbol& member : members)
    {
        typeCount = std::max(typeCount, member.declaringTypeOrdinal + 1U);
    }
    memberOrdinalsBySignature_.resize(typeCount);
    for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
    {
        const ClassMemberSymbol& member = members[ordinal];
        membersByOrdinal_.push_back(&member);
        memberOrdinalsBySignature_[member.declaringTypeOrdinal][member.signatureKey].push_back(ordinal);
    }
}

void ProjectCompilationIndexes::rebuildDispatch(
    std::span<const VirtualMethodInfo> virtualMethods,
    std::span<const ClassVirtualTable> classTables,
    std::size_t memberCount,
    std::size_t typeCount)
{
    virtualMethodsByMember_.assign(memberCount, nullptr);
    for (const VirtualMethodInfo& method : virtualMethods)
    {
        if (method.memberOrdinal >= virtualMethodsByMember_.size())
        {
            virtualMethodsByMember_.resize(method.memberOrdinal + 1U, nullptr);
        }
        virtualMethodsByMember_[method.memberOrdinal] = &method;
    }
    virtualTablesByType_.assign(typeCount, nullptr);
    for (const ClassVirtualTable& table : classTables)
    {
        if (table.typeOrdinal >= virtualTablesByType_.size())
        {
            virtualTablesByType_.resize(table.typeOrdinal + 1U, nullptr);
        }
        virtualTablesByType_[table.typeOrdinal] = &table;
    }
}

const NamedTypeDeclaration* ProjectCompilationIndexes::typeForOrdinal(
    std::size_t ordinal) const noexcept
{
    return ordinal < typesByOrdinal_.size() ? typesByOrdinal_[ordinal] : nullptr;
}

std::optional<std::size_t> ProjectCompilationIndexes::typeOrdinalForIdentity(
    std::string_view identity) const
{
    const auto iterator = typeOrdinalByIdentity_.find(std::string(identity));
    return iterator == typeOrdinalByIdentity_.end()
        ? std::nullopt : std::optional<std::size_t>(iterator->second);
}

std::span<const std::size_t> ProjectCompilationIndexes::typeOrdinalsForName(
    std::string_view name) const
{
    const auto iterator = typeOrdinalsByName_.find(std::string(name));
    return iterator == typeOrdinalsByName_.end()
        ? std::span<const std::size_t>{} : std::span<const std::size_t>(iterator->second);
}

const ClassMemberSymbol* ProjectCompilationIndexes::memberForOrdinal(
    std::size_t ordinal) const noexcept
{
    return ordinal < membersByOrdinal_.size() ? membersByOrdinal_[ordinal] : nullptr;
}

std::span<const std::size_t> ProjectCompilationIndexes::memberOrdinalsForSignature(
    std::size_t typeOrdinal,
    std::string_view signature) const noexcept
{
    if (typeOrdinal >= memberOrdinalsBySignature_.size())
    {
        return {};
    }
    const MemberSignatureIndex& index = memberOrdinalsBySignature_[typeOrdinal];
    const auto iterator = index.find(std::string(signature));
    return iterator == index.end()
        ? std::span<const std::size_t>{} : std::span<const std::size_t>(iterator->second);
}

const VirtualMethodInfo* ProjectCompilationIndexes::virtualMethodForMember(
    std::size_t memberOrdinal) const noexcept
{
    return memberOrdinal < virtualMethodsByMember_.size()
        ? virtualMethodsByMember_[memberOrdinal] : nullptr;
}

const ClassVirtualTable* ProjectCompilationIndexes::virtualTableForType(
    std::size_t typeOrdinal) const noexcept
{
    return typeOrdinal < virtualTablesByType_.size() ? virtualTablesByType_[typeOrdinal] : nullptr;
}
}
