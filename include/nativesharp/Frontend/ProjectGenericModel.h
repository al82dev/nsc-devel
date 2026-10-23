#pragma once

#include "nativesharp/Frontend/QualifiedNameLadder.h"
#include "nativesharp/Frontend/GenericInstantiationReport.h"
#include "nativesharp/Frontend/GenericSpecializationRecord.h"
#include "nativesharp/Frontend/GenericTelemetry.h"
#include "nativesharp/Frontend/ProjectCompilationIndexes.h"
#include "nativesharp/Frontend/GenericTemplateAccessibility.h"
#include "nativesharp/Frontend/GenericTemplateExternalTypeReference.h"
#include "nativesharp/Frontend/GenericTemplateKind.h"
#include "nativesharp/Frontend/GenericTemplateRecord.h"
#include "nativesharp/Frontend/InstantiationContext.h"
#include "nativesharp/Frontend/PrecompiledGenericSpecialization.h"
#include "nativesharp/Frontend/SpecializationState.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/OwnershipQualifierSyntaxKind.h"
#if 0 // INSTRUMENTATION CODE: detailed generics trace support.
#include "nativesharp/Basic/CompilationEvent.h"
#endif // INSTRUMENTATION CODE
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Basic/TransparentStringHash.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"
#include "nativesharp/Frontend/TemplateParseCache.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Language/GenericSemantics.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
struct LocatedNode;
struct TypeWork;

class ProjectGenericModel final
{
public:
    static constexpr std::size_t DefaultMaximumInstantiationDepth = 64;
    static constexpr std::size_t DefaultMaximumInstantiationCount = 16384;
    static constexpr std::size_t DefaultMaximumPackExpansionCount = 1024;

    explicit ProjectGenericModel(
        std::size_t maximumInstantiationDepth = DefaultMaximumInstantiationDepth,
        std::size_t maximumInstantiationCount = DefaultMaximumInstantiationCount) noexcept;
    ProjectGenericModel(
        ProjectCompilationIndexes& indexes,
        std::size_t maximumInstantiationDepth = DefaultMaximumInstantiationDepth,
        std::size_t maximumInstantiationCount = DefaultMaximumInstantiationCount) noexcept;

    bool build(
        const compilation::ProjectGraph& graph,
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        std::span<const PrecompiledGenericSpecialization> precompiledSpecializations,
        diagnostics::DiagnosticEngine& diagnostics);

    std::span<const GenericTemplateRecord> templates() const noexcept;
    std::span<const GenericSpecializationRecord> specializations() const noexcept;
    const GenericTemplateRecord* templateForCanonicalKey(
        std::string_view canonicalKey) const noexcept;
    const GenericSpecializationRecord* specializationForSyntax(
        const ast::AstNode* syntax) const noexcept;
    const GenericSpecializationRecord* specializationForTypeKey(
        std::string_view typeKey) const noexcept;
    // NS304-T03-F03. Construye la clave de tipo cerrada (`type:...Span__g1__<T>__h<16hex>`) de la
    // especializacion unaria de un template dado, para un TIPO ELEMENTO que ya esta resuelto como
    // clave (array/inline array). Reutiliza `sanitize` + `stableHashSuffix` (una sola fuente del
    // hash) y espeja el prefijo de `ProjectCompilation`. Devuelve vacio si el template no existe o
    // no es unario. Para tipos elemento definidos por el usuario la identidad se limita a su
    // spelling terminal (T11 completa la identidad de origen); para builtins coincide con la
    // identidad real.
    std::string closedTypeKeyForTemplate(
        std::string_view templateName,
        std::string_view elementTypeKey) const;
    // The recorded resolved identity for an open generic reference (`Box<T>`), or null when absent.
    const std::string* resolvedTypeIdentity(const ast::TypeSyntax* syntax) const noexcept;
    GenericInstantiationReport instantiationReport() const noexcept;
    // NSC305-P01. Sub-phase and counter telemetry for the generic pass. Always available; the
    // data is populated only when the compilation wired a timing sink (see compileProjectGenerics).
    GenericTelemetry& telemetry() noexcept;
    const GenericTelemetry& telemetry() const noexcept;
    bool isBuilt() const noexcept;
    // NSC305-C01. Sets the C1 template parse cache root (empty = disabled).
    void setTemplateCacheRoot(std::filesystem::path root);
    // NSC305-G09. Sets the requested worker count for the parallel specialization wave (1 = serial).
    void setParallelismWorkers(std::size_t workers) noexcept;
    // NSC305-C02. Warm-build restore of one template record: on a cache hit, deserializes the
    // metadata, links the walked syntax node and pushes the TemplateState (skipping the record
    // construction). Returns false on miss or disabled cache.
    bool tryRestoreTemplateRecord(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        ast::AstNode* parent,
        std::string_view currentNamespace,
        std::string_view ownerName);

    // NSC305-C01. Persists the collected template records to the C1 cache (no-op when disabled).
    void storeTemplateRecords();
    // NSC305-C01. Restores the collected template records from the C1 cache and counts hits (no-op
    // when disabled). The AST subtree is still walked; this validates + reports the metadata hit.
    void restoreTemplateRecords();

    // NSC305-C04. Stores a MIR cache record (key + interface/implementation fingerprints) under the
    // implementation fingerprint key (the C5 stable partition key). No-op when disabled.
    void storeMirCacheRecord(
        std::string_view mangledName,
        std::string_view genericSpecializationKey,
        std::string_view returnTypeKey,
        std::string_view errorTypeKey,
        const std::vector<std::string>& parameterTypeKeys,
        std::string_view interfaceFingerprint,
        std::string_view implementationFingerprint);
    // NSC305-C04. Restores a MIR cache record by implementation fingerprint. Returns false on miss or
    // disabled cache. Out-params: the interface fingerprint + the observable key fields.
    bool restoreMirCacheRecord(
        std::string_view implementationFingerprint,
        std::string& mangledName,
        std::string& genericSpecializationKey,
        std::string& returnTypeKey,
        std::string& errorTypeKey,
        std::vector<std::string>& parameterTypeKeys,
        std::string& interfaceFingerprint);

private:
    struct TemplateState final
    {
        GenericTemplateRecord record;
        ProjectSyntaxUnit* unit = nullptr;
        ast::AstNode* syntax = nullptr;
        ast::AstNode* parent = nullptr;
        // NSC-LIMIT-007. Su indice en templateStates_, que es el MISMO que en templates(): los
        // records se copian uno a uno y en orden. La validacion de constraints necesita ir de una
        // especializacion a su plantilla, y este es el enlace.
        std::size_t ordinal = 0;
    };

    // A lightweight predeclaration used only while the generic pass is choosing a cache key.
    // Full name binding deliberately still belongs to ProjectNameLookup, which runs after
    // monomorphization. This table answers the narrower question "which declaration does this
    // type argument name?" so one declaration keeps one generic identity across projects.
    struct DeclaredTypeIdentity final
    {
        std::size_t projectOrdinal = 0;
        std::string namespaceName;
        std::string sourceName;
        std::string identityKey;
        bool isPublic = false;
    };

    struct DeclaredTypeOrigin final
    {
        std::size_t projectOrdinal = 0;
        std::string namespaceName;
    };

    struct GenericLookupScope;
    struct DependencyFrame;
    struct BodyFrame;
    struct AliasFrame;
    struct ChildWork;
    struct CollectFrame;
    struct ProcessFrame;

    void collectTemplates(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        ast::AstNode* parent,
        std::string namespaceName,
        std::string ownerName,
        bool insideGenericType,
        diagnostics::DiagnosticEngine& diagnostics);
    void publishConstEnumType(
        const std::string& spelling,
        const std::string& enumTypeKey);
    void publishConstEnumValue(
        const std::string& spelling,
        const std::pair<std::string, std::string>& identity);
    void collectEnumTemplateData(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        const std::string& currentNamespace);
    void collectDeclaredTypeIdentity(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        const std::string& currentNamespace,
        const std::string& ownerName);
    void collectGenericTypeTemplate(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        ast::AstNode* parent,
        const std::string& currentNamespace,
        const std::string& ownerName,
        diagnostics::DiagnosticEngine& diagnostics);
    void collectGenericMethodTemplate(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        ast::AstNode* parent,
        const std::string& currentNamespace,
        const std::string& ownerName,
        bool insideGenericType,
        diagnostics::DiagnosticEngine& diagnostics);
    static void pushCollectTemplateChildren(
        ast::AstNode* node,
        const std::string& currentNamespace,
        const std::string& ownerName,
        bool insideGenericType,
        bool genericType,
        std::vector<CollectFrame>& stack);
    void processCollectTemplateFrame(
        ProjectSyntaxUnit& unit,
        CollectFrame frame,
        std::vector<CollectFrame>& stack,
        diagnostics::DiagnosticEngine& diagnostics);

    bool mergeTraitImplementations(
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics);
    void collectTraitImplementationNodes(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        const std::string& parentNamespace,
        std::vector<LocatedNode>& templates,
        std::vector<LocatedNode>& implementations);
    std::string genericParameterShape(const ast::AstNode* node) const;
    std::string genericConstraintShape(const ast::AstNode* node) const;
    ast::AstNode* findTraitImplementationOwner(
        const std::vector<LocatedNode>& templates,
        const LocatedNode& implementation) const;
    static bool traitImplementationHasDirectTarget(
        const ast::AstNode* implementation,
        const ast::TypeSyntax* target);
    bool mergeTraitImplementation(
        const std::vector<LocatedNode>& templates,
        const LocatedNode& implementation,
        diagnostics::DiagnosticEngine& diagnostics);
    bool collectAssociatedItems(
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics);
    void collectAssociatedItemNodes(
        ast::AstNode* node,
        const std::string& parentNamespace,
        const std::string& parentOwner,
        std::vector<LocatedNode>& traits,
        std::vector<LocatedNode>& owners);
    void collectAssociatedItemUnits(
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        std::vector<LocatedNode>& traits,
        std::vector<LocatedNode>& owners);
    bool indexTraitAssociatedItems(
        const std::vector<LocatedNode>& traits,
        std::unordered_map<std::string, const ast::AstNode*>& traitTypes,
        std::unordered_map<std::string, const ast::AstNode*>& traitConstants,
        std::unordered_map<std::string, const LocatedNode*>& traitsByName,
        diagnostics::DiagnosticEngine& diagnostics);
    static void collectDirectAssociatedItemKeys(
        const LocatedNode& owner,
        const std::string& ownerName,
        std::unordered_set<std::string>& directTypeKeys,
        std::unordered_set<std::string>& directConstantKeys);
    std::string associatedTraitName(
        const LocatedNode& owner,
        const ast::TypeSyntax* base,
        const std::unordered_map<std::string, const LocatedNode*>& traitsByName) const;
    static std::unordered_map<std::string, const ast::TypeSyntax*>
        associatedTraitSubstitutions(
            const LocatedNode& trait,
            const ast::TypeSyntax* base);
    bool inheritAssociatedTypes(
        const LocatedNode& owner,
        const std::string& ownerName,
        const std::string& traitName,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        const std::unordered_map<std::string, const ast::AstNode*>& traitTypes,
        const std::unordered_set<std::string>& directTypeKeys,
        diagnostics::DiagnosticEngine& diagnostics);
    bool inheritAssociatedConstants(
        const LocatedNode& owner,
        const std::string& ownerName,
        const std::string& traitName,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        const std::unordered_map<std::string, const ast::AstNode*>& traitConstants,
        const std::unordered_set<std::string>& directConstantKeys,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateAssociatedConstantRequirement(
        const LocatedNode& owner,
        const std::string& ownerName,
        const std::string& traitName,
        const ast::AstNode* requirement,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateAssociatedRequirements(
        const LocatedNode& owner,
        const std::string& ownerName,
        const std::string& traitName,
        const LocatedNode& trait,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        diagnostics::DiagnosticEngine& diagnostics);
    bool collectOwnerAssociatedItems(
        const LocatedNode& owner,
        const std::unordered_map<std::string, const ast::AstNode*>& traitTypes,
        const std::unordered_map<std::string, const ast::AstNode*>& traitConstants,
        const std::unordered_map<std::string, const LocatedNode*>& traitsByName,
        diagnostics::DiagnosticEngine& diagnostics);
    bool visitAssociatedProjection(
        const std::string& key,
        std::unordered_map<std::string, unsigned>& projectionState,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateAssociatedProjections(diagnostics::DiagnosticEngine& diagnostics);
    bool registerAssociatedItems(
        const ast::AstNode* owner,
        std::string_view ownerName,
        diagnostics::DiagnosticEngine* diagnostics);

    void discoverTemplateDependencies(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        std::string namespaceName,
        std::string ownerName,
        GenericLookupScope scope);
    void collectTemplateDependencies(
        TemplateState& root,
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        std::string namespaceName,
        std::string ownerName,
        GenericLookupScope scope,
        const ast::AstNode* rootSyntax);
    void addDependencyImportedNamespace(
        GenericLookupScope& scope,
        std::string candidate) const;
    void collectDependencyImports(
        const ast::AstNode* node,
        const std::string& currentNamespace,
        GenericLookupScope& scope) const;
    void collectBodyDependencyImports(
        const ast::AstNode* node,
        GenericLookupScope& scope) const;
    void collectTypeDependency(
        TemplateState& root,
        ProjectSyntaxUnit& unit,
        const ast::TypeSyntax* syntax,
        const std::string& namespaceName,
        const GenericLookupScope& scope);
    void collectCallDependency(
        TemplateState& root,
        ProjectSyntaxUnit& unit,
        const ast::AstNode* call,
        const std::string& namespaceName,
        const std::string& ownerName,
        const GenericLookupScope& scope);
    TemplateState* stateForSyntax(const ast::AstNode* syntax) noexcept;

    // LOS `using` VIGENTES EN EL SITIO, que esta fase tambien necesita.
    //
    // La escala de nombres tiene un escalon de namespaces importados y otro de alias, y sin ellos
    // `Caja.F<int>()` bajo un `using Otro;` no encuentra a su dueno y cae al camino sin dueno --el
    // que materializa cualquier plantilla del mismo nombre--. Se recogen como los recoge
    // ProjectNameLookup::bindScope, del mismo AST y en el mismo sitio.
    //
    // Con una diferencia honesta: alli el namespace importado se RESUELVE contra los namespaces
    // que lo encierran, y aqui no hay con que resolverlo. Se guardan las dos formas --la escrita y
    // la escrita bajo cada prefijo del namespace del sitio-- de modo que el escalon las pruebe
    // todas. Un import que no acierte no elige mal: no acierta y se baja al escalon siguiente.
    struct GenericLookupScope final
    {
        struct GenericLambda final
        {
            const ast::AstNode* templateSyntax = nullptr;
            ast::AstNode* declaration = nullptr;
            ast::AstNode* declarationContainer = nullptr;
        };
        // NSC305-G04. Copy-on-write wrapper so every scope container copies in O(1) (a shared_ptr
        // refcount) instead of deep-copying a map/vector on each traversal descent. `read()` is the
        // const view; `write()` clones the container only when it is shared, then returns a mutable
        // reference. Small lexical containers get cloned rarely, so the clone cost is negligible.
        template <typename T>
        struct Cow final
        {
            Cow() : value_(std::make_shared<T>()) {}
            const T& read() const { return *value_; }
            T& write()
            {
                if (value_.use_count() > 1)
                    value_ = std::make_shared<T>(*value_);
                return *value_;
            }
            std::shared_ptr<T> value_;
        };
        Cow<std::vector<std::string>> importedNamespaces;
        Cow<std::unordered_map<std::string, std::string>> aliases;
        // Item 276. Type aliases have to disappear before generic templates are registered:
        // otherwise `Alias<int>` is mistaken for a template identity of its own.  The declaration
        // carries both the target syntax and its optional type-parameter list.
        Cow<std::unordered_map<std::string, const ast::AstNode*>> typeAliases;
        Cow<std::unordered_set<std::string>> typeParameters;
        // NSC305-G04. Item 277. Types written on parameters, fields, and preceding locals are
        // sufficient for local generic inference. This container is the one that grows with the
        // number of locals, so it is a persistent parent-linked chain: copying a scope is O(1) and
        // adding a binding is O(1). Lookup walks newest-to-oldest, which preserves shadowing.
        struct ValueBinding final
        {
            ValueBinding(
                std::string bindingName,
                const ast::TypeSyntax* bindingType,
                std::shared_ptr<ValueBinding> bindingParent);

            ~ValueBinding() noexcept;

            std::string name;
            const ast::TypeSyntax* type = nullptr;
            std::shared_ptr<ValueBinding> parent;
        };
        std::shared_ptr<ValueBinding> valueTypes;
        void addValueType(std::string name, const ast::TypeSyntax* type);
        const ast::TypeSyntax* findValueType(std::string_view name) const noexcept;
        // By-reference parameter types carry a trailing Pointer modifier for ABI representation.
        // Preserve the source pass kind separately so structural pre-Sema type discovery can look
        // through that wrapper without losing the readonly meaning of `in`.
        Cow<std::unordered_map<std::string, std::string>> valuePassKinds;
        // MatchExpression lowers every arm receiver to MatchValueExpression. The generic prepass
        // carries the governing source type lexically so those synthetic receivers remain typed
        // before the definitive body model creates its single-evaluation storage.
        const ast::TypeSyntax* matchValueType = nullptr;
        std::string matchValuePassKind;
        // T08-F04. Generic lambdas are lexical templates, not members and therefore not entries
        // in the project-wide template catalogue. The generic pass closes each invocation in
        // the lexical scope where the `var` binding was introduced.
        Cow<std::unordered_map<std::string, GenericLambda>> genericLambdas;
        ast::AstNode* declarationContainer = nullptr;
        // Return type before closed generic spellings are rewritten. It provides expected-type
        // evidence to a direct generic call in a return statement.
        const ast::TypeSyntax* expectedReturnType = nullptr;
    };

    struct BodyTypeSpecializationDemand final
    {
        ProjectSyntaxUnit* unit = nullptr;
        ast::TypeSyntax* syntax = nullptr;
        GenericLookupScope scope;
        std::string namespaceName;
        std::string stableKey;
    };

    // NSC305-G08. A type specialization demand recorded by the collector (the two-pass discovery);
    // the target syntax is rewritten only in the apply phase. Executed later by the wave scheduler.
    struct GenericTypeDemand final
    {
        ProjectSyntaxUnit* unit = nullptr;
        TemplateState* state = nullptr;
        std::vector<ast::TypeSyntax*> arguments;
        ast::TypeSyntax* target = nullptr;
        std::string key;
        std::string specializedName;
        std::vector<std::string> identities;
        InstantiationContext context;
        // NSC305-G08. Logical instantiation depth (parent depth + 1), carried by the demand so the
        // wave scheduler preserves the depth metric without the C++ recursion stack.
        std::size_t logicalDepth = 1;
        // NSC305-G09. Stable sort key (project | namespace | source offset) so the wave is ordered
        // deterministically independent of discovery order.
        std::string stableKey;
    };

    struct ReservedDemand;

    // NSC305-G09. The private output of a specialization worker (the committer merges it by the
    // reserved ordinal). The clone lives in the worker's arena and is linked on merge.
    struct SpecializationProduct final
    {
        std::size_t ordinal = 0;
        // The worker owns every cloned node until the serial coordinator accepts the product.
        // AstArena::absorb transfers ownership without changing any published pointer.
        std::unique_ptr<ast::AstArena> arena;
        ast::AstNode* clone = nullptr;
        std::vector<GenericTypeDemand> childDemands;
        std::unordered_map<const ast::TypeSyntax*, std::string> resolvedTypeIdentities;
        std::unordered_map<const ast::TypeSyntax*, DeclaredTypeOrigin> resolvedTypeOrigins;
        bool success = false;
        // NSC305-G09. Causal clone errors captured on the worker thread, so the serial committer can
        // report them without reading the worker's thread-local state.
        std::string packError;
        SourceRange packErrorRange{};
        std::string associatedError;
        SourceRange associatedErrorRange{};
    };

    // NSC305-G07. Snapshot of the ten mutable "current instantiation" fields that
    // ensureTypeSpecialization/ensureMethodSpecialization save before materializing and restore
    // afterwards (they form a call stack, not global state). A nested struct can read the enclosing
    // class's private members, so the save/restore is a single two-line pair.
    struct SpecializationContext final
    {
        std::size_t projectOrdinal = 0;
        std::string namespaceName;
        SourceRange range{};
        std::size_t overloadGroup = 0;
        std::unordered_map<std::string, std::vector<const ast::TypeSyntax*>> packSubstitutions;
        std::string packError;
        SourceRange packErrorRange{};
        std::unordered_set<std::string> associatedProjections;
        std::string associatedError;
        SourceRange associatedErrorRange{};

        void save(const ProjectGenericModel& model);
        void restore(ProjectGenericModel& model) const;
    };

    void expandAliasesForBuild(
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics);
    bool mergeTraitImplementationsForBuild(
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics);
    bool buildOpaqueResultsForBuild(
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics);
    void indexOrdinaryCallableReturnTypes(
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units);
    bool collectAssociatedItemsForBuild(
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics);
    void collectTemplatesForBuild(
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics);
    void disambiguateTemplateCanonicalKeys();
    void buildTemplateIndexes();
    void discoverTemplateDependenciesForBuild(
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units);
    void publishTemplateRecordsForBuild(diagnostics::DiagnosticEngine& diagnostics);
    void seedImportedSpecializationsForBuild(
        std::span<const PrecompiledGenericSpecialization> precompiledSpecializations);
    void discoverRootDemandsForBuild(
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics);
    bool materializePendingDemandsForBuild(diagnostics::DiagnosticEngine& diagnostics);
    void finalizeBuildState();

    bool specializeGenericLambdaCall(
        ProjectSyntaxUnit& unit,
        ast::AstNode* call,
        const GenericLookupScope::GenericLambda& lambda,
        const GenericLookupScope& scope,
        diagnostics::DiagnosticEngine& diagnostics);
    bool collectExplicitGenericLambdaArguments(
        const ast::AstNode* call,
        const std::vector<ast::GenericParameterSyntax>& parameters,
        std::unordered_map<std::string, ast::TypeSyntax*>& inferred,
        std::unordered_map<std::string, std::vector<const ast::TypeSyntax*>>& packSubstitutions,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool inferGenericLambdaArgumentsFromValues(
        ProjectSyntaxUnit& unit,
        const ast::AstNode* call,
        const ast::AstNode* source,
        const GenericTemplateRecord& inferenceRecord,
        const std::unordered_set<std::string>& names,
        const GenericLookupScope& scope,
        std::unordered_map<std::string, ast::TypeSyntax*>& inferred,
        diagnostics::DiagnosticEngine& diagnostics);
    bool buildGenericLambdaSubstitutions(
        const ast::AstNode* call,
        const std::vector<ast::GenericParameterSyntax>& parameters,
        const std::unordered_map<std::string, ast::TypeSyntax*>& inferred,
        const std::unordered_map<std::string, std::vector<const ast::TypeSyntax*>>& packSubstitutions,
        std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void markGenericLambdaRecursiveCalls(ast::AstNode* node, const std::string& localName);
    void inferGenericLambdaReturnType(
        ProjectSyntaxUnit& unit,
        ast::AstNode* specialized,
        const GenericLookupScope& scope);
    std::size_t registerGenericLambdaTemplate(
        ProjectSyntaxUnit& unit,
        const ast::AstNode* source);
    GenericSpecializationRecord createGenericLambdaSpecialization(
        const ast::AstNode* call,
        const ast::AstNode* source,
        ast::AstNode* specialized,
        std::size_t templateOrdinal,
        const std::vector<ast::GenericParameterSyntax>& parameters,
        const std::unordered_map<std::string, std::vector<const ast::TypeSyntax*>>& packSubstitutions,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions) const;
    void appendGenericLambdaSpecializationArguments(
        const std::vector<ast::GenericParameterSyntax>& parameters,
        const std::unordered_map<std::string, std::vector<const ast::TypeSyntax*>>& packSubstitutions,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        GenericSpecializationRecord& specialization) const;
    std::optional<std::string> synthesizeGenericLambdaDelegate(
        ProjectSyntaxUnit& unit,
        const ast::AstNode* call,
        const GenericLookupScope::GenericLambda& local,
        const ast::AstNode* source,
        ast::AstNode* lambda,
        std::string_view suffix,
        diagnostics::DiagnosticEngine& diagnostics);
    bool rewriteGenericLambdaCall(
        ProjectSyntaxUnit& unit,
        ast::AstNode* call,
        const GenericLookupScope::GenericLambda& local,
        const ast::AstNode* source,
        ast::AstNode* specialized,
        const std::string& instanceKey,
        std::size_t instanceOrdinal,
        bool captures,
        diagnostics::DiagnosticEngine& diagnostics);
    bool materializeGenericLambdaCarrier(
        ProjectSyntaxUnit& unit,
        const ast::AstNode* call,
        const GenericLookupScope::GenericLambda& local,
        const ast::AstNode* source,
        ast::AstNode* specialized,
        const std::string& instanceKey,
        bool captures,
        diagnostics::DiagnosticEngine& diagnostics);

    bool expandAliasesInNode(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        std::string namespaceName,
        GenericLookupScope scope,
        diagnostics::DiagnosticEngine& diagnostics);
    bool expandAliasesForNode(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        const GenericLookupScope& scope,
        diagnostics::DiagnosticEngine& diagnostics);

    bool expandTypeAliases(
        ProjectSyntaxUnit& unit,
        ast::TypeSyntax* syntax,
        const GenericLookupScope& scope,
        diagnostics::DiagnosticEngine& diagnostics,
        std::vector<std::string>& expansionStack);
    bool applyExpandedAliasSyntax(
        ast::TypeSyntax* syntax,
        ast::TypeSyntax* expanded,
        const ast::AstNode* declaration,
        ast::OwnershipQualifierSyntaxKind writtenOwnership,
        diagnostics::DiagnosticEngine& diagnostics);

    ast::TypeSyntax* cloneAliasType(
        ast::AstArena& arena,
        const ast::TypeSyntax* source,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        bool& ownershipConflict);

    void processNode(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        std::string namespaceName,
        std::string ownerName,
        GenericLookupScope scope,
        bool insideTemplate,
        diagnostics::DiagnosticEngine& diagnostics);
    void scheduleProcessedNodeChildren(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        const std::string& currentNamespace,
        const std::string& ownerName,
        const GenericLookupScope& currentScope,
        std::vector<ProcessFrame>& frames);

    // NSC305-G08. Builds the lexical scope for one node (namespace, using directives/aliases,
    // expected return type, parameter/field snapshot) -- the discovery half of processNode.
    void prepareNodeScope(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        const std::string& namespaceName,
        const GenericLookupScope& scope,
        std::string& outNamespace,
        GenericLookupScope& outScope);
    // NSC305-G08. The per-node body of processNode (match/template/constraint/type/call/
    // inference); returns false when the node's children must be skipped (alias directive or
    // open template). Separating it leaves processNode as the iterative collector.
    bool processNodeOnce(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        const std::string& currentNamespace,
        const std::string& ownerName,
        GenericLookupScope& currentScope,
        bool insideTemplate,
        diagnostics::DiagnosticEngine& diagnostics);
    void prepareMatchExpressionScope(
        ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        GenericLookupScope& scope);
    void processNodeGenericConstraints(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        const std::string& currentNamespace,
        GenericLookupScope& currentScope,
        diagnostics::DiagnosticEngine& diagnostics);
    bool shouldSkipTemplateNode(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        const std::string& currentNamespace,
        GenericLookupScope& currentScope,
        bool templateNode,
        bool insideTemplate,
        diagnostics::DiagnosticEngine& diagnostics);
    ast::TypeSyntax* declarationExpectedType(
        ProjectSyntaxUnit& unit,
        const ast::AstNode* node);
    void processNodePrimaryTypeSyntax(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        const std::string& currentNamespace,
        GenericLookupScope& currentScope,
        diagnostics::DiagnosticEngine& diagnostics);
    void processNodeGenericCalls(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        const std::string& currentNamespace,
        const std::string& ownerName,
        GenericLookupScope& currentScope,
        diagnostics::DiagnosticEngine& diagnostics);
    void applyNodeExpectedType(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        const ast::TypeSyntax* expectedDeclarationType,
        const std::string& currentNamespace,
        const std::string& ownerName,
        GenericLookupScope& currentScope,
        diagnostics::DiagnosticEngine& diagnostics);

    void collectBodyTypeSpecializationDemand(
        ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        const std::string& namespaceName,
        const GenericLookupScope& scope);
    bool materializeBodyTypeSpecializationDemands(
        diagnostics::DiagnosticEngine& diagnostics);
    // NSC305-G09. Processes one wave of body-type demands (sorted by stable key, deduplicated).
    bool processBodyTypeDemandWave(
        std::vector<BodyTypeSpecializationDemand>& wave,
        diagnostics::DiagnosticEngine& diagnostics);

    bool processTypeSyntax(
        ProjectSyntaxUnit& unit,
        ast::TypeSyntax* syntax,
        const std::string& namespaceName,
        const GenericLookupScope& scope,
        diagnostics::DiagnosticEngine& diagnostics,
        // NSC305-G08. When non-null, receives the canonical specialization key of the materialized
        // type specialization (the SpecializationId that the demand-driven planner will use).
        std::string* outSpecializationKey = nullptr);
    std::optional<bool> processNestedTypeSyntax(
        ProjectSyntaxUnit& unit,
        ast::TypeSyntax* syntax,
        const std::string& namespaceName,
        const GenericLookupScope& scope,
        diagnostics::DiagnosticEngine& diagnostics);
    void recordDeclaredTypeIdentity(
        ast::TypeSyntax* syntax,
        std::size_t projectOrdinal,
        const std::string& namespaceName,
        const GenericLookupScope& scope);
    bool validateGenericTypeArgumentCount(
        const TemplateState& state,
        const ast::TypeSyntax* syntax,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool hasOpenGenericParameter(
        const ast::TypeSyntax* syntax,
        const GenericLookupScope& scope) const;
    // NSC305-G08. Materializes a resolved closed type and rewrites the source spelling (the
    // "rewrite" half of the demand/rewrite split). Returns false on error.
    bool materializeTypeSyntax(
        ProjectSyntaxUnit& unit,
        ast::TypeSyntax* syntax,
        TemplateState& state,
        const std::string& namespaceName,
        diagnostics::DiagnosticEngine& diagnostics,
        std::string* outSpecializationKey = nullptr);
    // NSC305-G08. Records a type specialization demand (the discovery half of the two-pass); the
    // apply half (executeTypeDemandWaves) materializes and rewrites it later.
    void collectTypeSpecializationDemand(
        ProjectSyntaxUnit& unit,
        ast::TypeSyntax* target,
        TemplateState& state,
        const std::string& namespaceName);
    // NSC305-G08. Executes the collected type demands in waves (the apply half of the two-pass).
    bool executeTypeDemandWaves(diagnostics::DiagnosticEngine& diagnostics);
    void applyTypeDemandContext(const GenericTypeDemand& demand);
    std::vector<ReservedDemand> reserveTypeDemandWave(
        std::vector<GenericTypeDemand>& wave,
        diagnostics::DiagnosticEngine& diagnostics);
    std::string applyReservedTypeDemand(
        ReservedDemand& reservedDemand,
        SpecializationProduct* preClonedProduct,
        diagnostics::DiagnosticEngine& diagnostics);
    void cloneReservedTypeDemands(
        std::vector<ReservedDemand>& reserved,
        std::vector<SpecializationProduct>& products);
    bool applyClonedTypeDemands(
        std::vector<ReservedDemand>& reserved,
        std::vector<SpecializationProduct>& products,
        diagnostics::DiagnosticEngine& diagnostics);
    bool applyReservedTypeDemands(
        std::vector<ReservedDemand>& reserved,
        diagnostics::DiagnosticEngine& diagnostics);
    bool executeTypeDemandWave(
        std::vector<GenericTypeDemand>& wave,
        diagnostics::DiagnosticEngine& diagnostics);

    bool processGenericCall(
        ProjectSyntaxUnit& unit,
        ast::AstNode* call,
        const std::string& namespaceName,
        const std::string& ownerName,
        const GenericLookupScope& scope,
        diagnostics::DiagnosticEngine& diagnostics);

    // NSC305-G08. Materializes every shared-overload candidate of a generic call with one common
    // specialized name (the "ID 132" path), then rewrites the call site. Returns false on error.
    bool materializeSharedOverloads(
        ProjectSyntaxUnit& unit,
        ast::AstNode* call,
        ast::AstNode* callee,
        const std::string& methodName,
        const std::string& namespaceName,
        const std::vector<TemplateState*>& shared,
        diagnostics::DiagnosticEngine& diagnostics);
    // NSC305-G08. Normalizes and materializes the single resolved method of a generic call, then
    // rewrites the call site. Returns false on error.
    bool materializeUniqueMethod(
        ProjectSyntaxUnit& unit,
        ast::AstNode* call,
        ast::AstNode* callee,
        TemplateState& state,
        const std::string& namespaceName,
        diagnostics::DiagnosticEngine& diagnostics);

    bool inferGenericCall(
        ProjectSyntaxUnit& unit,
        ast::AstNode* call,
        const std::string& namespaceName,
        const std::string& ownerName,
        const GenericLookupScope& scope,
        diagnostics::DiagnosticEngine& diagnostics,
        const ast::TypeSyntax* expectedType = nullptr,
        bool reportFailure = true,
        std::size_t inferenceDepth = 0);
    std::vector<TemplateState*> resolveGenericInferenceCandidates(
        const ProjectSyntaxUnit& unit,
        const std::string& namespaceName,
        const std::string& ownerName,
        const GenericLookupScope& scope,
        const ast::AstNode* callee,
        const std::string& methodName);
    void inferNestedGenericCallArguments(
        ProjectSyntaxUnit& unit,
        ast::AstNode* call,
        const std::string& namespaceName,
        const std::string& ownerName,
        const GenericLookupScope& scope,
        diagnostics::DiagnosticEngine& diagnostics,
        std::size_t inferenceDepth);
    bool hasApplicableNonGenericOverload(
        const std::vector<TemplateState*>& candidates,
        const ast::AstNode* call,
        const std::string& methodName,
        const ast::TypeSyntax* expectedType) const;
    std::size_t genericPatternSpecificity(
        const ast::TypeSyntax* pattern,
        const std::unordered_set<std::string>& parameters) const;
    bool mapGenericCallArguments(
        const ast::AstNode* call,
        const TemplateState& candidate,
        std::vector<const ast::AstNode*>& parameters,
        std::vector<std::optional<std::size_t>>& mapping,
        bool& extensionCandidate) const;
    bool inferGenericCandidateEvidence(
        ProjectSyntaxUnit& unit,
        const ast::AstNode* call,
        TemplateState& candidate,
        const std::vector<const ast::AstNode*>& parameters,
        const std::vector<std::optional<std::size_t>>& mapping,
        bool extensionCandidate,
        const std::unordered_set<std::string>& genericParameters,
        const GenericLookupScope& scope,
        const ast::TypeSyntax* expectedType,
        std::unordered_map<std::string, ast::TypeSyntax*>& inferred);
    bool applyGenericCandidateContext(
        ProjectSyntaxUnit& unit,
        ast::AstNode* call,
        TemplateState& candidate,
        const std::vector<const ast::AstNode*>& parameters,
        const std::vector<std::optional<std::size_t>>& mapping,
        const std::unordered_set<std::string>& genericParameters,
        const std::string& namespaceName,
        const std::string& ownerName,
        const GenericLookupScope& scope,
        diagnostics::DiagnosticEngine& diagnostics,
        std::size_t inferenceDepth,
        std::unordered_map<std::string, ast::TypeSyntax*>& inferred);
    bool inferGenericCandidate(
        ProjectSyntaxUnit& unit,
        ast::AstNode* call,
        TemplateState& candidate,
        std::size_t candidateCount,
        const std::string& namespaceName,
        const std::string& ownerName,
        const GenericLookupScope& scope,
        diagnostics::DiagnosticEngine& diagnostics,
        const ast::TypeSyntax* expectedType,
        std::size_t inferenceDepth,
        std::vector<ast::TypeSyntax*>& arguments,
        std::size_t& specificity);
    void collectGenericCallInferences(
        ProjectSyntaxUnit& unit,
        ast::AstNode* call,
        const std::vector<TemplateState*>& candidates,
        const std::string& namespaceName,
        const std::string& ownerName,
        const GenericLookupScope& scope,
        diagnostics::DiagnosticEngine& diagnostics,
        const ast::TypeSyntax* expectedType,
        std::size_t inferenceDepth,
        std::vector<std::tuple<
            TemplateState*, std::vector<ast::TypeSyntax*>, std::size_t>>& successes);
    void recordGenericInferenceReturnType(
        ProjectSyntaxUnit& unit,
        ast::AstNode* call,
        const std::vector<std::tuple<
            TemplateState*, std::vector<ast::TypeSyntax*>, std::size_t>>& successes);
    bool haveSameGenericInferenceArguments(
        const std::tuple<TemplateState*, std::vector<ast::TypeSyntax*>, std::size_t>& candidate,
        const std::tuple<TemplateState*, std::vector<ast::TypeSyntax*>, std::size_t>& first) const;
    bool commitGenericCallInference(
        ProjectSyntaxUnit& unit,
        ast::AstNode* call,
        const std::string& methodName,
        bool reportFailure,
        diagnostics::DiagnosticEngine& diagnostics,
        std::vector<std::tuple<
            TemplateState*, std::vector<ast::TypeSyntax*>, std::size_t>>& successes);

    bool applyExpectedTypeToExpression(
        ProjectSyntaxUnit& unit,
        ast::AstNode* expression,
        const ast::TypeSyntax* expectedType,
        const std::string& namespaceName,
        const std::string& ownerName,
        const GenericLookupScope& scope,
        diagnostics::DiagnosticEngine& diagnostics,
        std::size_t inferenceDepth = 0);

    bool inferLambdaTypeArguments(
        ProjectSyntaxUnit& unit,
        const ast::AstNode* lambda,
        const ast::TypeSyntax* delegatePattern,
        const GenericTemplateRecord& methodRecord,
        const std::unordered_set<std::string>& methodParameters,
        const GenericLookupScope& scope,
        std::unordered_map<std::string, ast::TypeSyntax*>& inferred);
    TemplateState* resolveLambdaDelegate(
        const ProjectSyntaxUnit& unit,
        const ast::TypeSyntax* delegatePattern);

    ast::TypeSyntax* inferExpressionType(
        ProjectSyntaxUnit& unit,
        const ast::AstNode* expression,
        const GenericLookupScope& scope);
    static ast::TypeSyntax* makeInferredBuiltinType(
        ProjectSyntaxUnit& unit,
        const ast::AstNode* expression,
        lexer::TokenKind kind,
        std::string name);
    ast::TypeSyntax* cloneInferredType(
        ProjectSyntaxUnit& unit,
        const ast::TypeSyntax* source);
    ast::TypeSyntax* inferNamedExpressionType(
        ProjectSyntaxUnit& unit,
        const ast::AstNode* expression,
        const GenericLookupScope& scope);
    ast::TypeSyntax* inferIntegerLiteralType(
        ProjectSyntaxUnit& unit,
        const ast::AstNode* expression);
    ast::TypeSyntax* inferConstructionExpressionType(
        ProjectSyntaxUnit& unit,
        const ast::AstNode* expression);
    ast::TypeSyntax* inferUnaryExpressionType(
        ProjectSyntaxUnit& unit,
        const ast::AstNode* expression,
        const GenericLookupScope& scope);
    ast::TypeSyntax* inferConditionalExpressionType(
        ProjectSyntaxUnit& unit,
        const ast::AstNode* expression,
        const GenericLookupScope& scope);
    static bool binaryExpressionReturnsBoolean(lexer::TokenKind tokenKind);
    ast::TypeSyntax* inferBinaryExpressionType(
        ProjectSyntaxUnit& unit,
        const ast::AstNode* expression,
        const GenericLookupScope& scope);
    ast::TypeSyntax* inferTupleExpressionType(
        ProjectSyntaxUnit& unit,
        const ast::AstNode* expression,
        const GenericLookupScope& scope);
    ast::TypeSyntax* inferCallExpressionType(
        ProjectSyntaxUnit& unit,
        const ast::AstNode* expression);

    bool inferTypeArguments(
        ProjectSyntaxUnit& unit,
        const ast::TypeSyntax* pattern,
        const ast::TypeSyntax* actual,
        const GenericTemplateRecord& record,
        const std::unordered_set<std::string>& parameters,
        std::unordered_map<std::string, ast::TypeSyntax*>& inferred);
    bool inferArrayExtentTypeArguments(
        ProjectSyntaxUnit& unit,
        const ast::TypeSyntax* pattern,
        const ast::TypeSyntax* actual,
        const GenericTemplateRecord& record,
        std::size_t modifierPrefix,
        std::unordered_map<std::string, ast::TypeSyntax*>& inferred);
    bool recordInferredTypeArgument(
        const std::string& parameterName,
        ast::TypeSyntax* value,
        std::unordered_map<std::string, ast::TypeSyntax*>& inferred) const;
    std::optional<bool> inferConstantTypeArgument(
        ProjectSyntaxUnit& unit,
        const ast::TypeSyntax* pattern,
        const ast::TypeSyntax* actual,
        const GenericTemplateRecord& record,
        const std::unordered_set<std::string>& parameters,
        std::unordered_map<std::string, ast::TypeSyntax*>& inferred);
    ast::TypeSyntax* cloneInferredTypeArgument(
        ProjectSyntaxUnit& unit,
        const ast::TypeSyntax* pattern,
        const ast::TypeSyntax* actual,
        std::size_t modifierPrefix);
    std::optional<bool> inferNamedTypeArgument(
        ProjectSyntaxUnit& unit,
        const ast::TypeSyntax* pattern,
        const ast::TypeSyntax* actual,
        const GenericTemplateRecord& record,
        const std::unordered_set<std::string>& parameters,
        std::size_t modifierPrefix,
        std::unordered_map<std::string, ast::TypeSyntax*>& inferred);

    bool normalizeGenericArguments(
        const GenericTemplateRecord& record,
        std::span<ast::TypeSyntax* const> arguments,
        SourceRange range,
        diagnostics::DiagnosticEngine& diagnostics);
    static bool validateGenericPackArgumentCount(
        std::size_t argumentCount,
        const std::optional<std::size_t>& packIndex,
        SourceRange range,
        diagnostics::DiagnosticEngine& diagnostics);
    std::string structuralConstGenericTypeKey(
        const ast::TypeSyntax* type) const;
    std::string expectedConstGenericTypeKey(
        const ast::GenericParameterSyntax& parameter) const;
    bool normalizeEnumConstGenericArgument(
        ast::TypeSyntax* argument,
        const std::string& expectedType,
        std::string& argumentType,
        std::string& argumentValue) const;
    bool validateConstGenericArgumentKind(
        const GenericTemplateRecord& record,
        const ast::GenericParameterSyntax& parameter,
        ast::GenericParameterSyntaxKind kind,
        std::size_t parameterIndex,
        ast::TypeSyntax* argument,
        bool constant,
        const std::string& expectedType,
        const std::string& constSubject,
        SourceRange range,
        diagnostics::DiagnosticEngine& diagnostics) const;
    static std::optional<bool> validateStructuralConstGenericArgument(
        const GenericTemplateRecord& record,
        const ast::GenericParameterSyntax& parameter,
        std::size_t parameterIndex,
        ast::TypeSyntax* argument,
        const std::string& expectedType,
        const std::string& argumentType,
        const std::string& argumentValue,
        const std::string& constSubject,
        diagnostics::DiagnosticEngine& diagnostics);
    static bool integralConstGenericValueFits(
        std::uint64_t value,
        unsigned expectedWidth,
        bool expectedSigned,
        bool argumentSigned);
    static bool normalizeIntegralConstGenericArgument(
        const GenericTemplateRecord& record,
        const ast::GenericParameterSyntax& parameter,
        std::size_t parameterIndex,
        ast::TypeSyntax* argument,
        const std::string& expectedType,
        const std::string& argumentType,
        const std::string& argumentValue,
        const std::string& constSubject,
        diagnostics::DiagnosticEngine& diagnostics);
    bool normalizeGenericArgument(
        const GenericTemplateRecord& record,
        std::span<ast::TypeSyntax* const> arguments,
        std::size_t index,
        const std::optional<std::size_t>& packIndex,
        SourceRange range,
        diagnostics::DiagnosticEngine& diagnostics);

    // NSC305-G02. Triestado de resolución de tipo: Missing (sin candidato), Unique (uno) o
    // Ambiguous (dos o más). Preserva la distinción para diagnóstico, sin cambiar los llamadores
    // que tratan "no único" uniformemente.
    enum class TypeResolutionKind : std::uint8_t
    {
        Missing = 0,
        Unique = 1,
        Ambiguous = 2,
    };
    struct TypeResolution final
    {
        TypeResolutionKind kind = TypeResolutionKind::Missing;
        TemplateState* state = nullptr;
    };

    TypeResolution resolveTypeTemplate(
        const ProjectSyntaxUnit& unit,
        std::string_view namespaceName,
        std::string_view writtenName,
        std::size_t arity);
    // LAS PLANTILLAS QUE UNA LLAMADA NOMBRA, y solo esas.
    //
    // Los tres filtros baratos --nombre, aridad generica y cuenta de argumentos de VALOR-- se
    // aplican siempre. `argumentCount` separa `Cuenta<T>(T[])` de `Cuenta<T>(T[], int, int)`; lo
    // que NO separa es `F<T>(T)` de `F<T>(T[])`, y eso es deliberado: elegir ahi necesita los
    // TIPOS, que esta fase no tiene, asi que se materializan las dos y decide la resolucion de
    // sobrecargas ordinaria.
    //
    // EL DUENO SE DECIDE CON LA ESCALA COMPARTIDA, no con reglas propias. `site` es el sitio de la
    // llamada en forma textual --namespace lexico, cadena de tipos contenedores, alias, imports--
    // y `qualifiedNameLadder` produce las escrituras candidatas EN ORDEN. Se devuelve el PRIMER
    // escalon que produzca alguna plantilla, que es exactamente lo que hace el modelo de nombres
    // con su tabla de tipos.
    //
    // Vacio significa «ningun escalon nombra un dueno». No significa «no hay plantillas»: puede
    // que el receptor escrito no sea un tipo sino un VALOR --`objeto.F<int>()`--, y esa distincion
    // no se puede hacer aqui. Quien recibe el vacio decide si cae al camino sin dueno.
    std::vector<TemplateState*> templatesNamedBy(
        const ProjectSyntaxUnit& unit,
        const QualifiedNameSite& site,
        std::string_view writtenOwner,
        std::string_view methodName,
        std::size_t arity,
        std::size_t argumentCount,
        const std::vector<QualifiedNameRung>* ladder = nullptr);

    // El conjunto de SOBRECARGAS que la llamada alcanza. Es `templatesNamedBy` mas la regla de
    // DUENO UNICO: si el escalon ganador abarca dos escrituras de dueno distintas la llamada es
    // ambigua de verdad y no se materializa nada, en vez de clonar plantillas de dos tipos
    // homonimos y analizar un cuerpo que este codigo no nombra.
    std::vector<TemplateState*> collectMethodTemplates(
        const ProjectSyntaxUnit& unit,
        const QualifiedNameSite& site,
        std::string_view writtenOwner,
        std::string_view methodName,
        std::size_t arity,
        std::size_t argumentCount);

    // NSC305-G03b. The single-owner filter of `collectMethodTemplates`, separated so a call site
    // that already computed the winning-rung candidates can reuse them instead of rebuilding the
    // name ladder.
    std::vector<TemplateState*> collectMethodTemplatesFromNamed(
        const std::vector<TemplateState*>& named) const;

    // La misma busqueda cuando se espera UNA sola plantilla. Usa `templatesNamedBy` --la misma
    // funcion, no una copia de sus reglas-- de modo que las dos no pueden divergir.
    //
    // El camino SIN dueno solo se toma cuando la escala no encontro ninguno, que es la unica
    // forma que tiene esta fase de sospechar que el receptor escrito era un valor y no un tipo.
    // Tomarlo cuando el dueno SI se resolvio seria deshacer el filtro que acaba de aplicarse.
    TemplateState* resolveMethodTemplate(
        const ProjectSyntaxUnit& unit,
        const QualifiedNameSite& site,
        std::string_view writtenOwner,
        std::string_view methodName,
        std::size_t arity,
        std::size_t argumentCount,
        const std::vector<TemplateState*>* named = nullptr,
        const std::vector<QualifiedNameRung>* ladder = nullptr);

    std::string ensureTypeSpecialization(
        TemplateState& state,
        std::span<ast::TypeSyntax* const> arguments,
        diagnostics::DiagnosticEngine& diagnostics,
        // NSC305 (G-series). When non-null, receives the canonical specialization key of the
        // materialized (or cache-hit) type specialization, so callers avoid a reverse scan of
        // `specializations_` by `specializedName`.
        std::string* outSpecializationKey = nullptr);
    bool restoreTypeSpecializationFromCache(
        TemplateState& state,
        const std::string& key,
        const std::string& specializedName,
        const std::vector<std::string>& identities,
        const SpecializationContext& callerContext,
        diagnostics::DiagnosticEngine& diagnostics);
    // NSC305-G09. Merges a produced specialization clone into its reserved slot: sets the terminal
    // name/metadata, attaches the clone to the template parent, and marks the record Ready.
    void commitTypeSpecialization(
        TemplateState& state,
        ast::AstNode* clone,
        const std::string& key,
        const std::string& specializedName,
        std::size_t ordinal,
        diagnostics::DiagnosticEngine& diagnostics);
    // NSC305-G09. Reserves a slot + ordinal for a type specialization (the "reserve" step of the
    // wave scheduler); the execute/merge steps fill it afterward.
    std::size_t reserveTypeSpecialization(
        const TemplateState& state,
        const std::string& key,
        const std::string& specializedName,
        const std::vector<std::string>& identities);
    // NSC305-G09. Executes a reserved type specialization: clones, commits the syntax, and
    // processes the clone. Returns false on error (the caller rolls back the reserved slot).
    bool executeTypeSpecialization(
        TemplateState& state,
        std::span<ast::TypeSyntax* const> arguments,
        const std::string& key,
        const std::string& specializedName,
        std::size_t ordinal,
        const SpecializationContext& callerContext,
        diagnostics::DiagnosticEngine& diagnostics);
    // NSC305-G09. Executes a reserved type specialization in a worker and returns the private
    // product (clone + child demands). The committer merges the product by the reserved ordinal.
    SpecializationProduct executeTypeSpecializationProduct(
        TemplateState& state,
        std::span<ast::TypeSyntax* const> arguments,
        const std::string& key,
        const std::string& specializedName,
        std::size_t ordinal,
        const SpecializationContext& callerContext,
        diagnostics::DiagnosticEngine& diagnostics);
    // NSC305-G09. Clones a type specialization on the worker thread (the parallel half) and captures
    // the causal errors in the product; the committer later reports + links + processes them.
    SpecializationProduct cloneTypeSpecializationProduct(
        TemplateState& state,
        std::span<ast::TypeSyntax* const> arguments,
        const std::string& specializedName,
        const SpecializationContext& callerContext);
    // NSC305-G09. The serial commit half of a specialization: reports clone errors, links + processes
    // the clone and stores it. Runs on the coordinator thread after the parallel clone.
    bool commitTypeSpecializationProduct(
        TemplateState& state,
        SpecializationProduct& product,
        const std::string& key,
        const std::string& specializedName,
        const SpecializationContext& callerContext,
        diagnostics::DiagnosticEngine& diagnostics);
    // NSC305-G09. Deterministic pre-reserve for the parallel wave: computes key/name, dedups,
    // restores from cache, enters the budget and reserves the ordinal. Returns SIZE_MAX on a hit or
    // error (the caller handles it); otherwise returns the reserved ordinal.
    std::size_t reserveTypeDemand(
        TemplateState& state,
        std::span<ast::TypeSyntax* const> arguments,
        std::string& key,
        std::string& specializedName,
        std::vector<std::string>& identities,
        diagnostics::DiagnosticEngine& diagnostics);
    // NSC305-G09. Executes a pre-reserved type demand (the apply half): C3 restore + execute + merge
    // + rewrite, using the ordinal/key/name computed by reserveTypeDemand. Returns the specialized
    // name (empty on failure).
    std::string executeTypeDemand(
        ProjectSyntaxUnit& unit,
        ast::TypeSyntax* syntax,
        TemplateState& state,
        const std::string& namespaceName,
        std::size_t ordinal,
        const std::string& key,
        const std::string& specializedName,
        std::span<ast::TypeSyntax* const> arguments,
        const SpecializationContext& callerContext,
        diagnostics::DiagnosticEngine& diagnostics,
        SpecializationProduct* preClonedProduct = nullptr);
    // NSC305-G09. Rewrites the target syntax after a specialization resolves (shared by the hit and
    // the execute paths): canonical identity + specialized base name + clear arguments.
    void rewriteSpecializedSyntax(
        ast::TypeSyntax* syntax,
        const GenericTemplateRecord& record,
        const std::string& key,
        const std::string& specializedName);
    std::string ensureMethodSpecialization(
        TemplateState& state,
        std::span<ast::TypeSyntax* const> arguments,
        diagnostics::DiagnosticEngine& diagnostics,
        // ID 132. Cuando varias plantillas comparten nombre y aridad generica, todas se
        // especializan con ESTE nombre y se convierten en sobrecargas ordinarias. Vacio para el
        // caso normal, donde el nombre lo deriva la propia clave.
        std::string_view sharedName = {});
    std::string methodSpecializedName(
        const TemplateState& state,
        std::size_t argumentCount,
        const std::vector<std::string>& spellings,
        const std::string& key,
        std::string_view sharedName) const;
    bool beginMethodSpecialization(
        const TemplateState& state,
        const std::string& key,
        diagnostics::DiagnosticEngine& diagnostics);
    std::size_t reserveMethodSpecialization(
        TemplateState& state,
        const std::string& key,
        const std::string& specializedName,
        const std::vector<std::string>& identities);
    ast::AstNode* cloneMethodSpecialization(
        TemplateState& state,
        std::span<ast::TypeSyntax* const> arguments,
        const SpecializationContext& callerContext,
        std::string& packError,
        SourceRange& packErrorRange,
        std::string& associatedError,
        SourceRange& associatedErrorRange);
    bool reportMethodSpecializationError(
        const std::string& key,
        const std::string& packError,
        const SourceRange& packErrorRange,
        const std::string& associatedError,
        const SourceRange& associatedErrorRange,
        const SpecializationContext& callerContext,
        diagnostics::DiagnosticEngine& diagnostics);
    void commitMethodSpecialization(
        TemplateState& state,
        ast::AstNode* clone,
        const std::string& key,
        const std::string& specializedName,
        std::size_t ordinal);
    std::string completeMethodSpecialization(
        TemplateState& state,
        ast::AstNode* clone,
        const std::string& key,
        const std::string& specializedName,
        std::size_t ordinal,
        const SpecializationContext& callerContext,
        diagnostics::DiagnosticEngine& diagnostics);

    // NSC305-G07. Shared steps of ensureTypeSpecialization/ensureMethodSpecialization so the two
    // no longer duplicate the key, spelling/identity and substitution/pack setup. Pure helpers;
    // behavior is unchanged.
    std::string computeSpecializationKey(
        const GenericTemplateRecord& record,
        std::span<ast::TypeSyntax* const> arguments,
        std::vector<std::string>& spellings,
        std::vector<std::string>& identities) const;
    void prepareSpecializationSubstitutions(
        TemplateState& state,
        std::span<ast::TypeSyntax* const> arguments,
        std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions);

    // NSC305-G08. Owns the vectors a QualifiedNameSite's spans point into, so the site can be built
    // once and returned by value (C++17 guaranteed elision keeps the spans valid).
    struct QualifiedNameSiteContext final
    {
        std::vector<std::string> namespaceSegments;
        std::vector<std::string> enclosingTypeNames;
        QualifiedNameSite site;
    };
    QualifiedNameSiteContext makeQualifiedNameSite(
        std::string_view namespaceName,
        std::string_view ownerName,
        const GenericLookupScope& scope) const;

    // NSC305-G08. Result of resolving a generic call site: the named templates at the winning rung,
    // the shared-overload set (single owner), and the unique fallback when there is one.
    struct GenericCallResolution final
    {
        std::vector<TemplateState*> named;
        std::vector<TemplateState*> shared;
        TemplateState* unique = nullptr;
    };
    GenericCallResolution resolveGenericCall(
        const ProjectSyntaxUnit& unit,
        const QualifiedNameSite& site,
        std::string_view requestedOwner,
        std::string_view methodName,
        std::size_t arity,
        std::size_t argumentCount);

    ast::TypeSyntax* cloneType(
        ast::AstArena& arena,
        const ast::TypeSyntax* source,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        std::size_t modifierLimit = std::numeric_limits<std::size_t>::max());
    ast::TypeSyntax* clonePackType(
        ast::AstArena& arena,
        const ast::TypeSyntax* source,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        std::size_t modifierLimit);
    ast::TypeSyntax* cloneSubstitutedOrGenericType(
        ast::AstArena& arena,
        const ast::TypeSyntax* source,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        std::size_t modifierLimit,
        bool& substituted);
    void appendCloneModifiers(
        ast::AstArena& arena,
        const ast::TypeSyntax* source,
        ast::TypeSyntax* result,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        std::size_t modifierLimit,
        bool updateIdentity);
    void applyClonedTypeSurface(
        const ast::TypeSyntax* source,
        ast::TypeSyntax* result,
        bool substituted);
    // NSC305-G06. The non-substitution branch of cloneType: creates the shallow result, clones the
    // function return/error types and type arguments, and carries forward origin tags.
    ast::TypeSyntax* cloneTypeGeneric(
        ast::AstArena& arena,
        const ast::TypeSyntax* source,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        std::size_t modifierLimit);
    bool isSpecialCloneType(
        const ast::TypeSyntax* syntax,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions) const;
    void seedCloneTypeWork(
        const ast::TypeSyntax* source,
        ast::TypeSyntax* result,
        std::vector<TypeWork>& work) const;
    ast::TypeSyntax* cloneTypeWorkItem(
        ast::AstArena& arena,
        const TypeWork& item,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        std::size_t modifierLimit,
        std::vector<TypeWork>& work);
    static void attachClonedType(const TypeWork& item, ast::TypeSyntax* child);
    // NSC305-G06. The shallow (non-recursive) part of the generic type clone: creates the result,
    // copies the function flags and carries forward origin tags. Return/error/argument cloning is
    // left to the caller.
    ast::TypeSyntax* cloneTypeGenericShallow(
        ast::AstArena& arena,
        const ast::TypeSyntax* source);
    // NSC305-G06. Normalizes an associated type projection (`T.Item`). Returns nullptr when the
    // source is not a projection, so cloneType falls through to the pack/substitution/generic path.
    ast::TypeSyntax* cloneProjectionType(
        ast::AstArena& arena,
        const ast::TypeSyntax* source,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        std::size_t modifierLimit);
    ast::TypeSyntax* cloneProjectionResult(
        ast::AstArena& arena,
        const ast::TypeSyntax* source,
        const ast::TypeSyntax* current,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        std::size_t modifierLimit);
    ast::TypeSyntax* recoverAssociatedProjectionFailure(
        ast::AstArena& arena,
        const ast::TypeSyntax* source,
        bool cycle,
        std::string_view detail);

    std::vector<ast::TypeSyntax*> cloneExpandedTypeArgument(
        ast::AstArena& arena,
        const ast::TypeSyntax* source,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions);
    void preserveOrAttachInstantiationOrigin(ast::TypeSyntax* syntax) const;
    ast::AstNode* cloneNode(
        ast::AstArena& arena,
        const ast::AstNode* source,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        std::string_view rewrittenContainingTypeName,
        std::string_view originalContainingTypeName);
    // NSC305-G06. Copies a node's direct scalar fields and modifiers into a fresh result of
    // `clonedKind` (the non-recursive part of the generic clone path).
    ast::AstNode* cloneNodeCopyFields(
        ast::AstArena& arena,
        const ast::AstNode* source,
        ast::AstNodeKind clonedKind);
    // NSC305-G06. The generic (non-special-case) clone of a node: fields, modifiers, generic
    // parameters, constraints, type syntax and throws -- but NOT the first/second/third/children
    // recursion, which the caller drives.
    ast::AstNode* cloneNodeGeneric(
        ast::AstArena& arena,
        const ast::AstNode* source,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        std::string_view rewrittenContainingTypeName,
        std::string_view originalContainingTypeName);
    void cloneGenericConstraints(
        ast::AstArena& arena,
        const ast::AstNode* source,
        ast::AstNode* result,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions);
    bool isLayoutQueryOfSubstitutedParameter(
        const ast::AstNode* node,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions) const;
    void cloneNodeChildrenIterative(
        ast::AstArena& arena,
        const ast::AstNode* source,
        ast::AstNode* result,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        std::string_view rewrittenContainingTypeName,
        std::string_view originalContainingTypeName);
    // NSC305-G06. Clones the special (non-generic) node cases -- member-expression associated
    // constant, value-pack fold, and identifier const substitution. Returns nullptr when the node
    // is not a special case so the caller falls through to the generic path.
    ast::AstNode* tryCloneSpecialNode(
        ast::AstArena& arena,
        const ast::AstNode* source,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        std::string_view rewrittenContainingTypeName,
        std::string_view originalContainingTypeName);
    // NSC305-G06. Folds a const value-pack operation (packcount/packat/packsum/packproduct/
    // packall/packany). Returns nullptr when the node is not such a fold, so cloneNode falls
    // through to the generic clone path.
    ast::AstNode* foldValuePack(
        ast::AstArena& arena,
        const ast::AstNode* source,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        std::string_view rewrittenContainingTypeName,
        std::string_view originalContainingTypeName);
    ast::AstNode* foldValuePackAt(
        ast::AstArena& arena,
        const ast::AstNode* source,
        const std::vector<const ast::TypeSyntax*>& pack,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        std::string_view rewrittenContainingTypeName,
        std::string_view originalContainingTypeName);
    ast::AstNode* foldValuePackReduction(
        ast::AstArena& arena,
        const ast::AstNode* source,
        const std::string& operation,
        const std::vector<const ast::TypeSyntax*>& pack);

    std::string typeSpelling(const ast::TypeSyntax* syntax) const;
    std::string functionTypeSpelling(const ast::TypeSyntax* syntax) const;
    std::string typeSpecializationIdentity(const ast::TypeSyntax* syntax) const;
    const std::string* lookupResolvedTypeIdentity(const ast::TypeSyntax* syntax) const noexcept;
    const DeclaredTypeOrigin* lookupResolvedTypeOrigin(const ast::TypeSyntax* syntax) const noexcept;
    std::string* mutableResolvedTypeIdentity(const ast::TypeSyntax* syntax);
    void recordResolvedTypeIdentity(const ast::TypeSyntax* syntax, std::string identity);
    void recordResolvedTypeOrigin(const ast::TypeSyntax* syntax, DeclaredTypeOrigin origin);
    const DeclaredTypeIdentity* resolveDeclaredTypeIdentity(
        const ast::TypeSyntax* syntax,
        std::size_t projectOrdinal,
        std::string_view namespaceName,
        const GenericLookupScope& scope) const;
    const DeclaredTypeIdentity* resolveModuleQualifiedDeclaredTypeIdentity(
        std::string_view writtenName,
        std::size_t projectOrdinal) const;
    static void appendTypeModifiers(std::string& identity, const ast::TypeSyntax* syntax);
    static std::string namespaceAppend(std::string_view prefix, std::string_view suffix);
    static std::string sanitize(std::string_view text);
    static std::string methodCalleeName(const ast::AstNode* call);
    static ast::AstNode* methodCalleeSyntax(ast::AstNode* call);
    void buildVisibility(const compilation::ProjectGraph& graph);
    bool projectVisible(std::size_t from, std::size_t target) const noexcept;
    bool templateVisible(std::size_t from, const GenericTemplateRecord& target) const noexcept;

    ProjectCompilationIndexes ownedIndexes_;
    ProjectCompilationIndexes* indexes_ = nullptr;
    std::vector<TemplateState> templateStates_;
    // NSC305-G01. O(1) lookups that replace the linear `stateForSyntax` scan and the
    // imported-specialization seed `find_if`. Populated after template collection (and, for
    // `stateByCanonicalKey_`, after canonical-key disambiguation). `templateStates_` is stable from
    // that point on, so the stored element pointers remain valid for the rest of build().
    std::unordered_map<const ast::AstNode*, TemplateState*> stateBySyntax_;
    std::unordered_map<std::string, TemplateState*> stateByCanonicalKey_;
    // NSC305-G02. Type-only candidate buckets for `resolveTypeTemplate`. The first maps the full
    // module-qualified spelling (`module::namespace.Outer.Inner`) to its type templates; the second
    // maps a terminal type name to its type templates. Both preserve source order and contain only
    // type templates (no methods), so a lookup examines homonyms instead of the whole catalog.
    std::unordered_map<
        std::string,
        std::vector<TemplateState*>,
        basic::TransparentStringHash,
        std::equal_to<>> typesByModuleQualified_;
    std::unordered_map<std::string, std::vector<TemplateState*>> typesByVisibleName_;
    // NSC305-G03. Method-only candidate bucket keyed by method name, in source order. Used by
    // `templatesNamedBy` and the owner-less `resolveMethodTemplate` path so a lookup examines
    // homonymous methods instead of the whole catalog.
    std::unordered_map<std::string, std::vector<TemplateState*>> methodsByName_;
    std::unordered_set<const ast::AstNode*> materializedGenericLambdas_;
    std::unordered_map<const ast::AstNode*, std::size_t> genericLambdaTemplateOrdinals_;
    std::unordered_map<const ast::AstNode*, std::string> genericLambdaCarrierKeys_;
    std::vector<DeclaredTypeIdentity> declaredTypeIdentities_;
    std::vector<GenericTemplateRecord> templateRecords_;
    std::vector<GenericSpecializationRecord> specializations_;
    // NSC305-G07. Deduplication and slot lifecycle for specialization demands. ensure*Specialization
    // delegates its cache hit/miss/commit/rollback here (the foundation for G09 waves).
    struct SpecializationPlanner final
    {
        std::unordered_map<std::string, std::size_t> byKey;
        std::optional<std::size_t> request(const std::string& key) const;
        void commit(const std::string& key, std::size_t ordinal);
        void rollback(const std::string& key);
    };
    SpecializationPlanner specializationPlanner_;
    std::unordered_map<const ast::AstNode*, std::size_t> specializationBySyntax_;
    std::unordered_map<const ast::TypeSyntax*, std::string> resolvedTypeIdentityBySyntax_;
    std::unordered_map<const ast::TypeSyntax*, DeclaredTypeOrigin> resolvedTypeOriginBySyntax_;
    std::unordered_map<const ast::AstNode*, ast::TypeSyntax*> inferredCallReturnTypes_;
    // Body-only syntax may request a closed type whose concrete receiver is known by the generic
    // prepass but not written as a TypeSyntax (list-pattern rest over an array is the first case).
    // Demands are collected during the serial AST walk and materialized in deterministic waves,
    // before name/member binding and before parallel method-body analysis.
    std::vector<BodyTypeSpecializationDemand> bodyTypeSpecializationDemands_;
    // NSC305-G08. Type specialization demands collected by the discovery pass and executed by the
    // wave (the two-pass foundation). Filled by the collector, drained by the executor.
    std::vector<GenericTypeDemand> typeSpecializationDemands_;
    // NSC305-G08. Logical instantiation depth of the wave currently being executed; the collector
    // records parent depth + 1 and the scheduler restores it per demand.
    std::size_t currentLogicalDepth_ = 0;
    // NSC305-G09. Requested worker count for the parallel wave (1 = serial); 0 is normalized to 1.
    std::size_t parallelismWorkers_ = 1;
    // NSC305-G09. O(1) dedup of body-type demands by stableKey (the "deduplicate demands by
    // SpecializationKey" wave step); cleared after each wave is processed.
    // Available before the ordinary member/body model. Only an unambiguous common return for a
    // non-generic callable name is retained; this lets an unnamed `some Trait` identity provide
    // evidence to generic inference without writing semantic state onto CallExpression AST nodes.
    std::unordered_map<std::string, const ast::TypeSyntax*> ordinaryCallableReturnTypes_;
    std::unordered_map<const ast::AstNode*, std::vector<const ast::AstNode*>>
        traitImplExtensions_;
    std::unordered_map<std::string, const ast::TypeSyntax*> associatedTypes_;
    std::unordered_map<std::string, const ast::AstNode*> associatedConstants_;
    // NSC305-G09. Per-worker (thread-local) clone/process state: a worker clones one specialization
    // on its own thread, so its pack/associated projection context, causal errors and clone depth
    // must not race with a sibling worker.
    thread_local static inline std::unordered_set<std::string> activeAssociatedProjections_;
    thread_local static inline std::string associatedProjectionError_;
    thread_local static inline SourceRange associatedProjectionErrorRange_{};
    std::vector<std::vector<std::size_t>> visibleProjects_;
    std::vector<std::string> projectModuleNames_;
    // Available before name binding because generic materialization itself runs before name
    // binding. Values use the enum's canonical declaration identity plus normalized underlying
    // bits, so aliases with the same value share one specialization.
    std::unordered_map<std::string, std::string> constEnumTypeBySpelling_;
    std::unordered_map<std::string, std::pair<std::string, std::string>> constEnumValueBySpelling_;
    bool built_ = false;
    // NSC305-C01. The C1 template parse cache (disk I/O). Root empty = disabled.
    TemplateParseCache templateCache_;

    // Stage 52 fix: the project ordinal and lexical namespace of whichever call site is
    // CURRENTLY requesting a specialization (set immediately before each
    // ensureTypeSpecialization/ensureMethodSpecialization call, in processTypeSyntax/
    // processGenericCall, where the requesting ProjectSyntaxUnit and namespace are available).
    // cloneType reads these when substituting a generic parameter for a plain (non-builtin,
    // non-generic) type argument. A successfully predeclared argument carries the DECLARATION
    // project/namespace from resolvedTypeOriginBySyntax_; these call-site values are the fallback
    // only for an argument that this early generic pass could not resolve canonically. This is
    // call-site state, not persistent instance state -- it is only ever read synchronously within
    // the ensureTypeSpecialization/ensureMethodSpecialization call that immediately follows each
    // assignment, never across a later, unrelated call.
    std::size_t currentInstantiationProjectOrdinal_ = 0;
    std::string currentInstantiationNamespace_;
    // NSC-LIMIT-007: donde se pidio la especializacion que se esta materializando. Viaja por el
    // mismo canal que el proyecto y el namespace de arriba, que es como esta fase ya pasaba
    // contexto del sitio a la funcion que materializa.
    SourceRange currentInstantiationRange_{};
    // NSC-LIMIT-007. El grupo de sobrecargas que se esta materializando, o 0 fuera de uno.
    std::size_t currentOverloadGroup_ = 0;
    std::size_t nextOverloadGroup_ = 0;
    language::InstantiationBudget instantiationBudget_;
    std::size_t instantiationCacheHitCount_ = 0;
    // NSC305-P01. Causal instrumentation for the generic pass; see GenericTelemetry.
    GenericTelemetry telemetry_;
    // NSC305-G09. Per-worker (thread-local) clone state, see the associated-projection members.
    thread_local static inline std::unordered_map<std::string, std::vector<const ast::TypeSyntax*>>
        activePackSubstitutions_;
    thread_local static inline std::string packMaterializationError_;
    thread_local static inline SourceRange packMaterializationErrorRange_{};
    // NSC305-G06. Recursion depth of the mutually-recursive cloneType/cloneNode pair, guarded so a
    // pathologically deep type/expression fails with a clean materialization error instead of
    // overflowing the process stack.
    thread_local static inline std::size_t cloneDepth_ = 0;
    // NSC305-G09 corrective. Worker clones cannot write the model-wide maps or read the mutable
    // coordinator context. These pointers redirect clone-local writes into the product and expose
    // the immutable request context on that worker only.
    thread_local static inline
        std::unordered_map<const ast::TypeSyntax*, std::string>* workerResolvedTypeIdentities_ = nullptr;
    thread_local static inline
        std::unordered_map<const ast::TypeSyntax*, DeclaredTypeOrigin>* workerResolvedTypeOrigins_ = nullptr;
    thread_local static inline const SpecializationContext* workerSpecializationContext_ = nullptr;
};

const char* genericTemplateKindName(GenericTemplateKind kind) noexcept;
}
