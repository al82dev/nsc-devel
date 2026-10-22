#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
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
struct MethodCleanupRegion;
struct MethodLambdaBinding;
struct MethodLambdaValueBinding;
struct MethodLocalSymbol;
struct NamedTypeDeclaration;
struct ObjectFieldLayout;
struct StructValueLayout;
struct VirtualMethodInfo;
struct ProjectOopMirFunction;

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
    void rebuildHirBindings(
        std::span<const MethodLocalSymbol> locals,
        std::span<const MethodLambdaBinding> lambdas,
        std::span<const MethodLambdaValueBinding> lambdaValues,
        std::span<const MethodCleanupRegion> cleanupRegions);
    void rebuildMirFunctions(
        std::span<const ProjectOopMirFunction> functions,
        std::size_t memberCount,
        std::size_t namespaceFunctionCount);
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
    const MethodLocalSymbol* hirLocalForSyntax(const ast::AstNode* syntax) const noexcept;
    const MethodLambdaBinding* hirLambdaForSyntax(const ast::AstNode* syntax) const noexcept;
    const MethodLambdaValueBinding* hirLambdaValueForSyntax(const ast::AstNode* syntax) const noexcept;
    const MethodCleanupRegion* hirCleanupRegionForSyntax(const ast::AstNode* syntax) const noexcept;
    const ProjectOopMirFunction* mirFunctionForMember(std::size_t memberOrdinal) const noexcept;
    const ProjectOopMirFunction* mirFunctionForNamespaceFunction(
        std::size_t functionOrdinal) const noexcept;
    const std::tuple<std::size_t, std::uint64_t, std::string>* mirAsyncLocalForOrdinal(
        const ProjectOopMirFunction& function,
        std::size_t localOrdinal) const noexcept;
    const std::tuple<std::size_t, std::uint64_t, std::string>* mirAsyncParameterForOrdinal(
        const ProjectOopMirFunction& function,
        std::size_t parameterOrdinal,
        std::string_view typeKey) const noexcept;

private:
    struct TransparentStringHash final
    {
        using is_transparent = void;

        std::size_t operator()(std::string_view value) const noexcept;
        std::size_t operator()(const std::string& value) const noexcept;
    };

    using MirAsyncFrameSlot = std::tuple<std::size_t, std::uint64_t, std::string>;
    using MirAsyncFrameIndex = std::unordered_multimap<std::size_t, const MirAsyncFrameSlot*>;
    using MemberSignatureIndex =
        std::unordered_map<std::string, std::vector<std::size_t>>;
    using TypeIdentityIndex = std::unordered_map<
        std::string,
        std::size_t,
        TransparentStringHash,
        std::equal_to<>>;
    using TypeNameIndex = std::unordered_map<
        std::string,
        std::vector<std::size_t>,
        TransparentStringHash,
        std::equal_to<>>;

    std::vector<const NamedTypeDeclaration*> typesByOrdinal_;
    TypeIdentityIndex typeOrdinalByIdentity_;
    TypeNameIndex typeOrdinalsByName_;
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
    std::unordered_map<const ast::AstNode*, const MethodLocalSymbol*> hirLocalBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodLambdaBinding*> hirLambdaBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodLambdaValueBinding*> hirLambdaValueBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodCleanupRegion*> hirCleanupRegionBySyntax_;
    std::vector<const ProjectOopMirFunction*> mirFunctionsByMember_;
    std::vector<const ProjectOopMirFunction*> mirFunctionsByNamespaceFunction_;
    std::unordered_map<const ProjectOopMirFunction*, MirAsyncFrameIndex> mirAsyncLocalsByFunction_;
    std::unordered_map<const ProjectOopMirFunction*, MirAsyncFrameIndex> mirAsyncParametersByFunction_;

    void clearHirBindings() noexcept;
    void clearMirFunctions() noexcept;
};
}
