#pragma once

#include "nativesharp/Basic/TransparentStringHash.h"

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
struct ObjectAllocationPlan;
struct ObjectFieldLayout;
struct ProjectOopMirInterfaceBinding;
struct StructValueLayout;
struct VirtualMethodInfo;
struct ProjectOopMirFunction;
struct ProjectOopMirClassLayout;
struct ProjectOopMirStructLayout;

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
        std::span<const ObjectAllocationPlan> allocationPlans,
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
    void rebuildMirLayouts(
        std::span<const ProjectOopMirStructLayout> structLayouts,
        std::span<const ProjectOopMirClassLayout> classLayouts,
        std::size_t typeCount);
    void rebuildMirInterfaces(
        std::span<const ProjectOopMirInterfaceBinding> interfaceBindings);
    bool registerBodyTypeDemand(std::string key);
    void clearBodyTypeDemands() noexcept;
    void registerGenericTypeDemand(std::string key);
    void clearGenericTypeDemands() noexcept;
    bool hasTypeIndex() const noexcept;
    bool hasLayoutIndexes() const noexcept;
    bool hasMirLayoutIndexes() const noexcept;
    const NamedTypeDeclaration* typeForOrdinal(std::size_t ordinal) const noexcept;
    std::optional<std::size_t> typeOrdinalForIdentity(std::string_view identity) const;
    std::span<const std::size_t> typeOrdinalsForName(std::string_view name) const;
    const ClassMemberSymbol* memberForOrdinal(std::size_t ordinal) const noexcept;
    std::span<const std::size_t> memberOrdinalsForName(
        std::string_view name) const noexcept;
    std::span<const std::size_t> memberOrdinalsForSignature(
        std::size_t typeOrdinal,
        std::string_view signature) const noexcept;
    const VirtualMethodInfo* virtualMethodForMember(std::size_t memberOrdinal) const noexcept;
    const ClassVirtualTable* virtualTableForType(std::size_t typeOrdinal) const noexcept;
    const ClassObjectLayout* classLayoutForType(std::size_t typeOrdinal) const noexcept;
    const StructValueLayout* structLayoutForType(std::size_t typeOrdinal) const noexcept;
    const ObjectFieldLayout* fieldLayoutForMember(std::size_t memberOrdinal) const noexcept;
    const ObjectAllocationPlan* allocationPlanForConstruction(
        std::size_t constructionOrdinal) const noexcept;
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
    bool hasHirOwnerIndexes() const noexcept;
    std::span<const std::size_t> hirLocalOrdinalsForMember(
        std::size_t memberOrdinal) const noexcept;
    bool hasHirLambdaIndexes() const noexcept;
    std::span<const std::size_t> hirLocalOrdinalsForLambda(
        std::size_t lambdaOrdinal) const noexcept;
    bool hasHirCleanupOwnerIndex() const noexcept;
    std::span<const std::size_t> hirCleanupRegionOrdinalsForMember(
        std::size_t memberOrdinal) const noexcept;
    const MethodLocalSymbol* hirLocalForSyntax(const ast::AstNode* syntax) const noexcept;
    const MethodLambdaBinding* hirLambdaForSyntax(const ast::AstNode* syntax) const noexcept;
    const MethodLambdaValueBinding* hirLambdaValueForSyntax(const ast::AstNode* syntax) const noexcept;
    const MethodCleanupRegion* hirCleanupRegionForSyntax(const ast::AstNode* syntax) const noexcept;
    const ProjectOopMirFunction* mirFunctionForMember(std::size_t memberOrdinal) const noexcept;
    const ProjectOopMirFunction* mirFunctionForNamespaceFunction(
        std::size_t functionOrdinal) const noexcept;
    const ProjectOopMirFunction* mirFunctionForMangledName(
        std::string_view mangledName) const noexcept;
    const ProjectOopMirFunction* mirFunctionForLambda(
        std::size_t lambdaOrdinal) const noexcept;
    const ProjectOopMirStructLayout* mirStructLayoutForType(
        std::size_t typeOrdinal) const noexcept;
    const ProjectOopMirStructLayout* mirStructLayoutForTypeKey(
        std::string_view typeKey) const noexcept;
    const ProjectOopMirClassLayout* mirClassLayoutForType(
        std::size_t typeOrdinal) const noexcept;
    const ProjectOopMirClassLayout* mirClassLayoutForTypeKey(
        std::string_view typeKey) const noexcept;
    const ProjectOopMirInterfaceBinding* mirInterfaceBindingForTypes(
        std::size_t implementingTypeOrdinal,
        std::size_t interfaceTypeOrdinal) const noexcept;
    bool mirInterfaceSlotForMember(
        std::size_t interfaceMemberOrdinal,
        std::uint64_t& stableInterfaceTypeId,
        std::size_t& slot) const noexcept;
    const std::tuple<std::size_t, std::uint64_t, std::string>* mirAsyncLocalForOrdinal(
        const ProjectOopMirFunction& function,
        std::size_t localOrdinal) const noexcept;
    const std::tuple<std::size_t, std::uint64_t, std::string>* mirAsyncParameterForOrdinal(
        const ProjectOopMirFunction& function,
        std::size_t parameterOrdinal,
        std::string_view typeKey) const noexcept;

private:
    using TransparentStringHash = basic::TransparentStringHash;

    using MirAsyncFrameSlot = std::tuple<std::size_t, std::uint64_t, std::string>;
    using MirAsyncFrameIndex = std::unordered_multimap<std::size_t, const MirAsyncFrameSlot*>;
    struct MirInterfaceSlot final
    {
        std::uint64_t stableInterfaceTypeId = 0;
        std::size_t slot = 0;
    };
    template <typename Value>
    using StringIndex = std::unordered_map<
        std::string,
        Value,
        TransparentStringHash,
        std::equal_to<>>;

    using MemberSignatureIndex = StringIndex<std::vector<std::size_t>>;
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
    bool typeIndexBuilt_ = false;
    std::vector<const ClassMemberSymbol*> membersByOrdinal_;
    StringIndex<std::vector<std::size_t>> memberOrdinalsByName_;
    std::vector<MemberSignatureIndex> memberOrdinalsBySignature_;
    std::vector<const VirtualMethodInfo*> virtualMethodsByMember_;
    std::vector<const ClassVirtualTable*> virtualTablesByType_;
    std::vector<const ClassObjectLayout*> classLayoutsByType_;
    std::vector<const StructValueLayout*> structLayoutsByType_;
    std::vector<const ObjectFieldLayout*> fieldLayoutsByMember_;
    std::vector<const ObjectAllocationPlan*> allocationPlansByConstruction_;
    bool layoutIndexesBuilt_ = false;
    std::vector<const GenericTemplateRecord*> genericTemplatesByOrdinal_;
    StringIndex<const GenericTemplateRecord*> genericTemplateByCanonicalKey_;
    StringIndex<std::vector<std::size_t>> genericTemplateOrdinalsByName_;
    std::vector<const GenericSpecializationRecord*> genericSpecializationsByOrdinal_;
    std::unordered_map<const ast::AstNode*, const GenericSpecializationRecord*>
        genericSpecializationBySyntax_;
    StringIndex<const GenericSpecializationRecord*> genericSpecializationByKey_;
    StringIndex<const GenericSpecializationRecord*> genericSpecializationByName_;
    std::unordered_set<std::string> bodyTypeDemandKeys_;
    std::unordered_set<std::string> genericTypeDemandKeys_;
    bool hirOwnerIndexesBuilt_ = false;
    std::vector<std::vector<std::size_t>> hirLocalOrdinalsByMember_;
    bool hirLambdaIndexesBuilt_ = false;
    std::vector<std::vector<std::size_t>> hirLocalOrdinalsByLambda_;
    bool hirCleanupOwnerIndexBuilt_ = false;
    std::vector<std::vector<std::size_t>> hirCleanupRegionOrdinalsByMember_;
    std::unordered_map<const ast::AstNode*, const MethodLocalSymbol*> hirLocalBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodLambdaBinding*> hirLambdaBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodLambdaValueBinding*> hirLambdaValueBySyntax_;
    std::unordered_map<const ast::AstNode*, const MethodCleanupRegion*> hirCleanupRegionBySyntax_;
    std::vector<const ProjectOopMirFunction*> mirFunctionsByMember_;
    std::vector<const ProjectOopMirFunction*> mirFunctionsByNamespaceFunction_;
    StringIndex<const ProjectOopMirFunction*> mirFunctionsByMangledName_;
    std::vector<const ProjectOopMirFunction*> mirFunctionsByLambda_;
    std::vector<const ProjectOopMirStructLayout*> mirStructLayoutsByType_;
    StringIndex<const ProjectOopMirStructLayout*> mirStructLayoutsByTypeKey_;
    std::vector<const ProjectOopMirClassLayout*> mirClassLayoutsByType_;
    StringIndex<const ProjectOopMirClassLayout*> mirClassLayoutsByTypeKey_;
    bool mirLayoutIndexesBuilt_ = false;
    std::vector<std::unordered_map<std::size_t, const ProjectOopMirInterfaceBinding*>>
        mirInterfaceBindingsByImplementingType_;
    std::unordered_map<std::size_t, MirInterfaceSlot> mirInterfaceSlotsByMember_;
    std::unordered_map<const ProjectOopMirFunction*, MirAsyncFrameIndex> mirAsyncLocalsByFunction_;
    std::unordered_map<const ProjectOopMirFunction*, MirAsyncFrameIndex> mirAsyncParametersByFunction_;

    void clearHirBindings() noexcept;
    void clearMirFunctions() noexcept;
};
}
