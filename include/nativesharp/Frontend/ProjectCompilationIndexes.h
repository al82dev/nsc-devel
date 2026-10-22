#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nativesharp::ast
{
class AstNode;
}

namespace nativesharp::frontend
{
struct ClassMemberSymbol;
struct ClassObjectLayout;
struct ClassVirtualTable;
struct GenericSpecializationRecord;
struct GenericTemplateRecord;
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
    void rebuildGenerics(
        std::span<const GenericTemplateRecord> templates,
        std::span<const GenericSpecializationRecord> specializations);
    bool registerBodyTypeDemand(std::string key);
    void clearBodyTypeDemands() noexcept;
    void registerGenericTypeDemand(std::string key);
    void clearGenericTypeDemands() noexcept;
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
    const GenericTemplateRecord* genericTemplateForOrdinal(std::size_t ordinal) const noexcept;
    const GenericTemplateRecord* genericTemplateForCanonicalKey(
        std::string_view canonicalKey) const noexcept;
    std::span<const std::size_t> genericTemplateOrdinalsForName(
        std::string_view name) const noexcept;
    const GenericSpecializationRecord* genericSpecializationForOrdinal(
        std::size_t ordinal) const noexcept;
    const GenericSpecializationRecord* genericSpecializationForSyntax(
        const ast::AstNode* syntax) const noexcept;
    const GenericSpecializationRecord* genericSpecializationForKey(
        std::string_view key) const noexcept;
    const GenericSpecializationRecord* genericSpecializationForName(
        std::string_view name) const noexcept;

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
    std::vector<const GenericTemplateRecord*> genericTemplatesByOrdinal_;
    std::unordered_map<std::string, const GenericTemplateRecord*> genericTemplateByCanonicalKey_;
    std::unordered_map<std::string, std::vector<std::size_t>> genericTemplateOrdinalsByName_;
    std::vector<const GenericSpecializationRecord*> genericSpecializationsByOrdinal_;
    std::unordered_map<const ast::AstNode*, const GenericSpecializationRecord*>
        genericSpecializationBySyntax_;
    std::unordered_map<std::string, const GenericSpecializationRecord*> genericSpecializationByKey_;
    std::unordered_map<std::string, const GenericSpecializationRecord*> genericSpecializationByName_;
    std::unordered_set<std::string> bodyTypeDemandKeys_;
    std::unordered_set<std::string> genericTypeDemandKeys_;
};
}
