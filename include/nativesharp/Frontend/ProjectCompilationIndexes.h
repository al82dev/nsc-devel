#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nativesharp::frontend
{
struct ClassMemberSymbol;
struct ClassObjectLayout;
struct ClassVirtualTable;
struct NamedTypeDeclaration;
struct ObjectFieldLayout;
struct StructValueLayout;
struct VirtualMethodInfo;

class ProjectCompilationIndexes final
{
public:
    void clear() noexcept;
    void rebuildTypes(std::span<const NamedTypeDeclaration> types);
    void rebuildMembers(std::span<const ClassMemberSymbol> members);
    void rebuildDispatch(
        std::span<const VirtualMethodInfo> virtualMethods,
        std::span<const ClassVirtualTable> classTables,
        std::size_t memberCount,
        std::size_t typeCount);
    void rebuildLayouts(
        std::span<const ClassObjectLayout> classLayouts,
        std::span<const StructValueLayout> structLayouts,
        std::span<const ObjectFieldLayout> promotedFields,
        std::size_t memberCount,
        std::size_t typeCount);
    const NamedTypeDeclaration* typeForOrdinal(std::size_t ordinal) const noexcept;
    std::optional<std::size_t> typeOrdinalForIdentity(std::string_view identity) const;
    std::span<const std::size_t> typeOrdinalsForName(std::string_view name) const;
    const ClassMemberSymbol* memberForOrdinal(std::size_t ordinal) const noexcept;
    std::span<const std::size_t> memberOrdinalsForSignature(
        std::size_t typeOrdinal,
        std::string_view signature) const noexcept;
    const VirtualMethodInfo* virtualMethodForMember(std::size_t memberOrdinal) const noexcept;
    const ClassVirtualTable* virtualTableForType(std::size_t typeOrdinal) const noexcept;
    const ClassObjectLayout* classLayoutForType(std::size_t typeOrdinal) const noexcept;
    const StructValueLayout* structLayoutForType(std::size_t typeOrdinal) const noexcept;
    const ObjectFieldLayout* fieldLayoutForMember(std::size_t memberOrdinal) const noexcept;

private:
    using MemberSignatureIndex =
        std::unordered_map<std::string, std::vector<std::size_t>>;

    std::vector<const NamedTypeDeclaration*> typesByOrdinal_;
    std::unordered_map<std::string, std::size_t> typeOrdinalByIdentity_;
    std::unordered_map<std::string, std::vector<std::size_t>> typeOrdinalsByName_;
    std::vector<const ClassMemberSymbol*> membersByOrdinal_;
    std::vector<MemberSignatureIndex> memberOrdinalsBySignature_;
    std::vector<const VirtualMethodInfo*> virtualMethodsByMember_;
    std::vector<const ClassVirtualTable*> virtualTablesByType_;
    std::vector<const ClassObjectLayout*> classLayoutsByType_;
    std::vector<const StructValueLayout*> structLayoutsByType_;
    std::vector<const ObjectFieldLayout*> fieldLayoutsByMember_;
};
}
