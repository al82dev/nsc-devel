#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Frontend/ClassMemberKind.h"
#include "nativesharp/Frontend/ClassMemberAccessibility.h"
#include "nativesharp/Frontend/ParameterPassKind.h"
#include "nativesharp/Frontend/MethodParameterSymbol.h"
#include "nativesharp/Frontend/ClassMemberSymbol.h"
#include "nativesharp/Frontend/ModuleInitializationRecord.h"
#include "nativesharp/Frontend/PrecompiledMemberIdentity.h"
#include "nativesharp/Frontend/MemberBodyBindingKind.h"
#include "nativesharp/Frontend/MemberBodyBinding.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"
#include "nativesharp/Frontend/InliningPolicy.h"
#include "nativesharp/Language/OwnershipSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Sema/OwnershipKind.h"
#include "nativesharp/Sema/EffectContext.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::compilation
{
class ProjectGraph;
}

namespace nativesharp::frontend
{
enum class AnonymousMemberPromotionVisitState : std::uint8_t;
class ProjectGenericModel;
const char* parameterPassKindName(ParameterPassKind kind) noexcept;
const char* parameterPassKindKeyword(ParameterPassKind kind) noexcept;

class ProjectMemberModel final
{
public:
    bool build(
        const ProjectNameLookup& nameLookup,
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics,
        const ProjectGenericModel* genericModel = nullptr);
    bool bindPrecompiledPhysicalNames(
        std::span<const PrecompiledMemberIdentity> identities,
        std::span<const std::pair<std::string, std::string>> typeKeyAliases,
        std::string& error);
    bool buildModuleInitializationPlan(
        const compilation::ProjectGraph& graph,
        std::string& error);

    std::span<const ClassMemberSymbol> members() const noexcept;
    std::span<const ModuleInitializationRecord> moduleInitializers() const noexcept;
    std::span<const MemberBodyBinding> bodyBindings() const noexcept;
    std::span<const std::size_t> membersOfType(std::size_t typeOrdinal) const noexcept;
    std::span<const std::size_t> constructorsOfType(std::size_t typeOrdinal) const noexcept;
    std::span<const std::size_t> promotedMembersOfType(std::size_t typeOrdinal) const noexcept;
    std::span<const std::size_t> findMembers(
        std::size_t typeOrdinal,
        std::string_view name) const noexcept;
    bool isBuilt() const noexcept;
    std::string canonicalTypeKey(
        const ast::TypeSyntax* syntax,
        const ProjectNameLookup& nameLookup) const;
    static std::string canonicalGenericArgumentKey(
        std::string_view spelling,
        const ProjectNameLookup& nameLookup);

    // True when a by-value type contains an `owned` instance field at any depth. Fixed arrays
    // are traversed through their element type. Reference-like and scalar types return false:
    // their bits do not carry an implicit release obligation unless the field itself is marked
    // `owned`, which callers check separately.
    bool typeTransitivelyOwnsStorage(std::string_view typeKey) const;

    // NativeSharp 3.0 T06-F02. A custom value destructor is a semantic barrier for partial
    // moves: it is written against the complete value and cannot observe a fabricated mixture
    // of live and moved fields. The query is transitive through tuples, fixed arrays and struct
    // fields so one outer drop flag can guard an entire value tree.
    bool typeTransitivelyHasCustomDestructor(std::string_view typeKey) const;
    bool typeRequiresValueDrop(std::string_view typeKey) const;

    // Scalars and reference-shaped values remain Copy. Structs are move-only unless their
    // declaration carries the explicit `copy` contract. Fixed arrays inherit the element
    // capability, so constraints, calls and method-body flow all ask one canonical service.
    bool typeIsImplicitlyCopyable(std::string_view typeKey) const;
    bool typeIsMoveOnly(std::string_view typeKey) const;

    // C2.1 "fixed arrays". The compile-time value of a `T[N]` count expression, or nullopt when
    // it is not a constant. It lives here, next to canonicalTypeKey, because the CANONICAL TYPE
    // KEY is the first consumer: a fixed-array key must read `builtin:float[4]`, with the count
    // itself in it, so that every later stage (element type, element size, ABI classification)
    // can answer questions about the type from the key alone. Before this, the key carried an
    // opaque fingerprint of the count's SYNTAX (`builtin:float[38:4:3:4]`), which nothing
    // downstream could interpret -- which is why indexing such a field never lowered.
    // ProjectObjectLayoutModel delegates to this so there is exactly one definition of "what
    // counts as a constant array count" in the compiler.
    static std::optional<std::uint64_t> evaluateConstantArrayCount(const ast::AstNode* expression) noexcept;

    // C2.1: the element type key of a fixed-array key ("builtin:float[4]" -> "builtin:float"),
    // and its count. Both return nullopt for anything that is not a fixed array -- a dynamic
    // array ("T[]"), a pointer, or a plain type.
    static std::optional<std::string> fixedArrayElementTypeKey(std::string_view typeKey);
    static std::optional<std::uint64_t> fixedArrayCount(std::string_view typeKey);
    static std::optional<std::string> flexibleArrayElementTypeKey(std::string_view typeKey);

private:
    friend struct ProjectOopHirModelTestAccess;

    const std::string* resolvedGenericTypeIdentity(
        const ast::TypeSyntax* syntax) const;
    std::string canonicalFunctionTypeKey(
        const ast::TypeSyntax* syntax,
        const ProjectNameLookup& nameLookup) const;
    std::string canonicalTupleTypeKey(
        const ast::TypeSyntax* syntax,
        const ProjectNameLookup& nameLookup) const;
    std::string canonicalPortableSimdTypeKey(
        const ast::TypeSyntax* syntax,
        const ProjectNameLookup& nameLookup) const;
    std::string canonicalNamedTypeKey(
        const ast::TypeSyntax* syntax,
        const ProjectNameLookup& nameLookup) const;
    void appendCanonicalTypeModifier(
        std::string& result,
        const ast::TypeModifierSyntax& modifier) const;
    void appendCanonicalTypeModifiers(
        std::string& result,
        const ast::TypeSyntax* syntax) const;
    static void applyCanonicalReferenceQualifier(
        std::string& result,
        const ast::TypeSyntax* syntax);

    struct TransparentStringHash final
    {
        using is_transparent = void;

        std::size_t operator()(std::string_view value) const noexcept;

        std::size_t operator()(const std::string& value) const noexcept;
    };

    using MemberNameIndex = std::unordered_map<
        std::string,
        std::vector<std::size_t>,
        TransparentStringHash,
        std::equal_to<>>;

    std::string syntaxFingerprint(const ast::AstNode* node) const;
    std::optional<std::size_t> typeOrdinalForKey(std::string_view typeKey) const noexcept;
    void initializeBuildState(
        const ProjectNameLookup& nameLookup,
        const ProjectGenericModel* genericModel);
    void validateDelegateParameters(
        const NamedTypeDeclaration& type,
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateExternDelegate(
        const NamedTypeDeclaration& type,
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateDelegateType(
        const NamedTypeDeclaration& type,
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void collectDeclaredTypeMembers(
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics);
    void collectExternalTraitImplMembers(
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics);
    void validateCopyType(
        std::size_t typeOrdinal,
        const NamedTypeDeclaration& type,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateCopyTypes(
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateRawUnion(
        std::size_t typeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateRawUnions(
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateCAbiExportNames(
        diagnostics::DiagnosticEngine& diagnostics) const;
    static std::string importedCSignature(const ClassMemberSymbol& member);
    void collectImportedCLibraries(
        std::unordered_map<
            std::string,
            std::pair<const ClassMemberSymbol*, std::string>>& importsByLibraryAndSymbol,
        std::unordered_map<std::string, std::vector<std::string>>& librariesBySymbol,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void markRuntimeBoundImports(
        const std::unordered_map<std::string, std::vector<std::string>>& librariesBySymbol,
        diagnostics::DiagnosticEngine& diagnostics);
    void validateImportedCIdentities(
        diagnostics::DiagnosticEngine& diagnostics);
    void scanMemberBodies(diagnostics::DiagnosticEngine& diagnostics);
    void scanCompilationUnits(
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void buildTransitiveOwnershipCache();
    void buildMemberNameIndex();
    bool collectTypeMembers(
        std::size_t typeOrdinal,
        const NamedTypeDeclaration& type,
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics);
    void validateTypeLifetimeConstraints(
        const NamedTypeDeclaration& type,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateExportedGenericMember(
        const ast::AstNode* directMember,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void registerTypeMemberDeclaration(
        const ast::AstNode* declaredMember,
        const NamedTypeDeclaration& type,
        std::unordered_map<std::string, const ast::AstNode*>& propertyDeclarations,
        std::unordered_map<std::string, const ast::AstNode*>& ordinaryDeclarations,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void collectExpandedPartMembers(
        const NamedTypeDeclarationPart& part,
        const NamedTypeDeclaration& type,
        std::unordered_map<std::string, const ast::AstNode*>& propertyDeclarations,
        std::unordered_map<std::string, const ast::AstNode*>& ordinaryDeclarations,
        std::vector<const ast::AstNode*>& expandedMembers,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void collectTypePartMembers(
        std::size_t typeOrdinal,
        const NamedTypeDeclaration& type,
        std::size_t partOrdinal,
        const NamedTypeDeclarationPart& part,
        const ProjectNameLookup& nameLookup,
        std::unordered_map<std::string, std::vector<std::size_t>>& byName,
        std::unordered_map<std::string, const ast::AstNode*>& propertyDeclarations,
        std::unordered_map<std::string, const ast::AstNode*>& ordinaryDeclarations,
        diagnostics::DiagnosticEngine& diagnostics);
    static ClassMemberKind classMemberKind(const ast::AstNode* memberNode) noexcept;
    void initializeMemberIdentity(
        ClassMemberSymbol& symbol,
        std::size_t typeOrdinal,
        std::size_t partOrdinal,
        std::size_t declarationOrdinal,
        const NamedTypeDeclarationPart& part,
        const ast::AstNode* memberNode) const;
    void applyMemberGenericSpecialization(
        ClassMemberSymbol& symbol,
        const ast::AstNode* memberNode) const;
    void applyMemberOrigin(
        ClassMemberSymbol& symbol,
        const NamedTypeDeclarationPart& part,
        const ast::AstNode* memberNode) const;
    bool initializeMemberDeclaredType(
        ClassMemberSymbol& symbol,
        const NamedTypeDeclaration& type,
        const ast::AstNode* memberNode,
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void initializeMemberPointerAndOwnership(
        ClassMemberSymbol& symbol,
        const ast::AstNode* memberNode) const;
    void initializeMemberLifetimeContract(
        ClassMemberSymbol& symbol,
        const ast::AstNode* memberNode) const;
    void validateMemberReferenceContract(
        const ClassMemberSymbol& symbol,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateMemberFieldLifetime(
        const ClassMemberSymbol& symbol,
        const NamedTypeDeclaration& type,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateMemberViewField(
        const ClassMemberSymbol& symbol,
        const NamedTypeDeclaration& type,
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void initializeMemberEffects(
        ClassMemberSymbol& symbol,
        const ast::AstNode* memberNode,
        bool isAsyncMethod) const;
    void initializeMemberStorageModifiers(
        ClassMemberSymbol& symbol,
        const NamedTypeDeclaration& type,
        const ast::AstNode* memberNode) const;
    void validateVolatileMember(
        const ClassMemberSymbol& symbol,
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateReadonlyStructMember(
        const ClassMemberSymbol& symbol,
        const NamedTypeDeclaration& type,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateStaticOwnedMember(
        const ClassMemberSymbol& symbol,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void initializeMemberDispatchAndInterop(
        ClassMemberSymbol& symbol,
        const NamedTypeDeclaration& type,
        const ast::AstNode* memberNode,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void applyExplicitMemberLayout(
        ClassMemberSymbol& symbol,
        const NamedTypeDeclaration& type,
        const ast::AstNode* memberNode,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void initializeGeneratedMemberMetadata(
        ClassMemberSymbol& symbol,
        const ast::AstNode* memberNode) const;
    void initializeAccessorMetadata(ClassMemberSymbol& symbol) const;
    void validateEventMember(
        const ClassMemberSymbol& symbol,
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void initializeMethodParameter(
        MethodParameterSymbol& parameter,
        const ast::AstNode* parameterNode,
        const NamedTypeDeclarationPart& part,
        const ProjectNameLookup& nameLookup) const;
    void applyFfiParameterMarkers(
        MethodParameterSymbol& parameter,
        const ast::AstNode* parameterNode) const;
    void validateParameterPack(
        MethodParameterSymbol& parameter,
        const ProjectNameLookup& nameLookup,
        bool& sawParameterPack,
        diagnostics::DiagnosticEngine& diagnostics) const;
    static void applyParameterPassKind(
        MethodParameterSymbol& parameter,
        const ast::AstNode* parameterNode);
    void validateParameterLifetime(
        MethodParameterSymbol& parameter,
        const ClassMemberSymbol& symbol,
        const NamedTypeDeclaration& type,
        const ast::AstNode* parameterNode,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool defaultParameterValueIsCompatible(
        const MethodParameterSymbol& parameter,
        const ast::AstNode* valueSyntax,
        const ProjectNameLookup& nameLookup) const;
    void validateDefaultParameter(
        const MethodParameterSymbol& parameter,
        bool& sawOptionalParameter,
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateCallableParameterIdentity(
        const MethodParameterSymbol& parameter,
        const ClassMemberSymbol& symbol,
        std::unordered_map<std::string, const ast::AstNode*>& parameterNames,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void collectCallableParameters(
        ClassMemberSymbol& symbol,
        const NamedTypeDeclaration& type,
        const NamedTypeDeclarationPart& part,
        const ast::AstNode* memberNode,
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateFfiParameterContract(
        ClassMemberSymbol& symbol,
        std::size_t parameterOrdinal,
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateMemberFfiContracts(
        ClassMemberSymbol& symbol,
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void inferReferenceReturnLifetime(
        ClassMemberSymbol& symbol,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateReferenceReturnLifetime(
        ClassMemberSymbol& symbol,
        const NamedTypeDeclaration& type,
        const ast::AstNode* memberNode,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void buildCallableSignature(
        ClassMemberSymbol& symbol,
        diagnostics::DiagnosticEngine& diagnostics) const;
    static void markAccessorOwnership(ClassMemberSymbol& symbol);
    void finalizeCallableMember(
        ClassMemberSymbol& symbol,
        const NamedTypeDeclaration& type,
        const ast::AstNode* memberNode,
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateInlineArraySignature(
        const ClassMemberSymbol& symbol,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateCAbiMember(
        const ClassMemberSymbol& symbol,
        const NamedTypeDeclaration& type,
        const ast::AstNode* memberNode,
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateUserOperator(
        const ClassMemberSymbol& symbol,
        const NamedTypeDeclaration& type,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateUserConversion(
        const ClassMemberSymbol& symbol,
        const NamedTypeDeclaration& type,
        diagnostics::DiagnosticEngine& diagnostics) const;
    static bool isPartialEventMerge(
        const ClassMemberSymbol& symbol,
        const ClassMemberSymbol& previous) noexcept;
    static bool isPartialMethodMerge(
        const ClassMemberSymbol& symbol,
        const ClassMemberSymbol& previous) noexcept;
    void mergePartialMethod(
        const ClassMemberSymbol& symbol,
        const ClassMemberSymbol& previous,
        const NamedTypeDeclaration& type,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool controlledSpecializationsMayCoexist(
        const ClassMemberSymbol& symbol,
        const ClassMemberSymbol& previous) const;
    bool memberConflictsWithPrevious(
        const ClassMemberSymbol& symbol,
        const ClassMemberSymbol& previous,
        const NamedTypeDeclaration& type,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool isDuplicateMember(
        const ClassMemberSymbol& symbol,
        const NamedTypeDeclaration& type,
        const std::unordered_map<std::string, std::vector<std::size_t>>& byName,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void appendCollectedMember(
        ClassMemberSymbol symbol,
        std::size_t typeOrdinal,
        const ast::AstNode* memberNode,
        std::unordered_map<std::string, std::vector<std::size_t>>& byName);
    void collectTypeMember(
        std::size_t typeOrdinal,
        const NamedTypeDeclaration& type,
        std::size_t partOrdinal,
        const NamedTypeDeclarationPart& part,
        const ast::AstNode* memberNode,
        std::size_t declarationOrdinal,
        const ProjectNameLookup& nameLookup,
        std::unordered_map<std::string, std::vector<std::size_t>>& byName,
        diagnostics::DiagnosticEngine& diagnostics);
    bool collectEnumMembers(
        std::size_t typeOrdinal,
        const NamedTypeDeclaration& type,
        diagnostics::DiagnosticEngine& diagnostics);
    bool collectAnonymousMemberPromotions(
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics);
    static ClassMemberAccessibility intersectAnonymousMemberAccessibility(
        ClassMemberAccessibility carrier,
        ClassMemberAccessibility member) noexcept;
    void promoteAnonymousSourceMember(
        std::size_t typeOrdinal,
        std::size_t carrierOrdinal,
        std::size_t sourceOrdinal,
        const ProjectNameLookup& nameLookup,
        std::unordered_map<std::string, std::size_t>& directNames,
        std::unordered_map<std::string, std::size_t>& promotedNames,
        diagnostics::DiagnosticEngine& diagnostics);
    void promoteAnonymousCarrierMembers(
        std::size_t typeOrdinal,
        std::size_t carrierOrdinal,
        const ProjectNameLookup& nameLookup,
        std::unordered_map<std::string, std::size_t>& directNames,
        std::unordered_map<std::string, std::size_t>& promotedNames,
        std::vector<AnonymousMemberPromotionVisitState>& states,
        diagnostics::DiagnosticEngine& diagnostics);
    void visitAnonymousMemberPromotions(
        std::size_t typeOrdinal,
        const ProjectNameLookup& nameLookup,
        std::vector<AnonymousMemberPromotionVisitState>& states,
        diagnostics::DiagnosticEngine& diagnostics);
    void scanMethodBody(
        const ast::AstNode* node,
        std::size_t typeOrdinal,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void completeThisMemberAccess(
        const ast::AstNode* node,
        std::size_t typeOrdinal,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void recordThisReference(
        const ast::AstNode* node,
        std::size_t typeOrdinal,
        std::size_t methodMemberOrdinal);
    static bool isDirectThisMemberAccess(const ast::AstNode* node) noexcept;
    static bool rejectThisPointerMemberAccess(
        const ast::AstNode* node,
        diagnostics::DiagnosticEngine& diagnostics);
    void scanInvalidThisContexts(
        const ast::AstNode* node,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void reportDuplicate(
        const ClassMemberSymbol& current,
        const ClassMemberSymbol& previous,
        diagnostics::DiagnosticEngine& diagnostics,
        std::string message) const;

    const ProjectNameLookup* nameLookup_ = nullptr;
    const ProjectGenericModel* genericModel_ = nullptr;
    std::vector<ClassMemberSymbol> members_;
    std::vector<ModuleInitializationRecord> moduleInitializers_;
    std::vector<MemberBodyBinding> bodyBindings_;
    std::vector<std::vector<std::size_t>> membersByType_;
    std::vector<std::vector<std::size_t>> constructorsByType_;
    std::vector<std::vector<std::size_t>> promotedMembersByType_;
    std::vector<MemberNameIndex> membersByName_;
    std::unordered_map<
        std::string,
        std::size_t,
        TransparentStringHash,
        std::equal_to<>> typeOrdinalByKey_;
    std::vector<std::uint8_t> transitiveOwnershipCache_;
    std::unordered_map<const ast::AstNode*, std::size_t> memberBySyntax_;
    bool built_ = false;
};

const char* classMemberKindName(ClassMemberKind kind) noexcept;
const char* classMemberAccessibilityName(ClassMemberAccessibility accessibility) noexcept;
const char* memberBodyBindingKindName(MemberBodyBindingKind kind) noexcept;
}
