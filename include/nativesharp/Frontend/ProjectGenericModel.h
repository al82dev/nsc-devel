#pragma once

#include "nativesharp/Frontend/QualifiedNameLadder.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Basic/SourceRange.h"
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
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
enum class GenericTemplateKind
{
    Class,
    Struct,
    Delegate,
    Method,
    // Appended to preserve the serialized numeric values of the four 1.0.1 kinds in NSMETA.
    Interface
};

enum class GenericTemplateAccessibility
{
    Private,
    Protected,
    Internal,
    Public
};
struct GenericTemplateExternalTypeReference final
{
    SourceRange range;
    // Canonical named identity, including its owning module. Modifiers are not part of this key;
    // the original template source keeps pointer/array syntax around the rewritten base name.
    std::string typeKey;
};

struct GenericTemplateRecord final
{
    GenericTemplateKind kind = GenericTemplateKind::Class;
    GenericTemplateAccessibility accessibility = GenericTemplateAccessibility::Internal;
    const ast::AstNode* syntax = nullptr;
    std::size_t projectOrdinal = 0;
    std::size_t sourceOrdinal = 0;
    std::string moduleName;
    std::string namespaceName;
    // NSC-LIMIT-009. La CADENA de tipos contenedores, unida con `+`: `A+Inner`, no `Inner`. Para
    // un metodo generico es su dueno; para un tipo generico, sus contenedores.
    std::string ownerName;
    std::string name;
    // El nombre del tipo con su cadena delante --`A+Inner`-- o el nombre a secas si no esta
    // anidado. Es lo que hace que dos `Inner<T>` bajo contenedores distintos no compartan clave.
    std::string qualifiedTypeName;
    std::vector<std::string> parameters;
    std::vector<ast::GenericParameterSyntax> parameterSyntaxes;
    // NSC-LIMIT-007. Las clausulas `where` DE LA PLANTILLA, copiadas del AST sin resolver. Se
    // guardan aqui y no se validan al recolectar porque cuando esta fase corre --la segunda de
    // toda la cadena-- todavia no existen el name lookup, la herencia ni las interfaces. Quien
    // valida es ProjectGenericConstraintModel, con las tres ya construidas.
    std::vector<ast::GenericConstraintClause> constraints;
    std::string canonicalKey;
    // NativeSharp 3.0 T04-F06. Methods enter one controlled-specialization family only when
    // their generic arity, receiver form and alpha-normalized value-parameter pattern agree.
    // Constraints are deliberately excluded: they define the partial order inside the family.
    std::string specializationFamilyKey;
    bool isControlledSpecialization = false;
    bool isControlledSpecializationFamily = false;
    // Canonical identities reached from this template's declaration/body. The producer uses
    // these edges to compute the reusable fixed-point closure; NSMETA persists them so artifact
    // verification can prove that every same-module dependency is present.
    std::vector<std::string> dependencies;
    // Ordinary named types reached from this template through its original using/alias scope.
    // These references let NSMETA make the declaration self-contained instead of depending on
    // source-level using directives that are intentionally not transported with a template body.
    std::vector<GenericTemplateExternalTypeReference> externalTypeReferences;

    // T05-F02. Source-level impl blocks merged into this template. They are retained separately
    // so NSMETA can publish the original class declaration followed by its coherence extensions;
    // a consumer then runs the same merge before monomorphization.
    std::vector<const ast::AstNode*> extensionSyntaxes;

    // Explicit generic uses that could not be resolved while inspecting the template. They are
    // harmless in an unreachable private template, but make a public reusable closure invalid.
    std::vector<std::string> unresolvedDependencies;

    // NSC305-C01. SHA-256 of the template's source text (reconstructed from the lexer tokens), the
    // C1 cache key input that invalidates on any source change (including the body).
    std::string sourceFingerprint;
    // NSC305-C01. Full C1 cache key: SHA-256 of (source fingerprint + cache schema version).
    std::string c1CacheKey;
};

// NSC305-G07. Lifecycle of a specialization slot. The sequential pass only uses Unseen (no slot),
// Building (record pushed, materializing) and Ready (product committed). Queued and Failed are the
// states the deterministic parallel planner (G09) will add.
enum class SpecializationState : std::uint8_t
{
    Unseen = 0,
    Queued = 1,
    Building = 2,
    Ready = 3,
    Failed = 4,
};

// NSC305-G09 (§6.4). The demand-owned instantiation context: what a specialization needs to know
// about its request site, carried by the demand itself instead of the shared model state, so a
// private worker can execute it without the C++ call stack.
struct InstantiationContext final
{
    std::size_t projectOrdinal = 0;
    std::string namespaceName;
    SourceRange range{};
    std::size_t overloadGroup = 0;
    std::unordered_map<std::string, std::vector<const ast::TypeSyntax*>> packSubstitutions;
    std::unordered_set<std::string> associatedProjections;
};

struct GenericSpecializationRecord final
{
    GenericTemplateKind kind = GenericTemplateKind::Class;
    const ast::AstNode* templateSyntax = nullptr;
    const ast::AstNode* specializedSyntax = nullptr;
    std::string templateKey;
    std::string specializationKey;
    std::string specializedName;
    std::vector<std::string> typeArguments;
    std::size_t cacheOrdinal = 0;
    // NSC-LIMIT-007. Lo que la validacion de constraints necesita y no podia deducir: DE QUE
    // PLANTILLA salio esta especializacion, y DONDE se pidio.
    //
    // El sitio importa mas de lo que parece: sin el, un `Caja<int>` invalido se diagnostica sobre
    // la declaracion de `Caja<T>`, que es correcta, y el lector va al sitio equivocado. El
    // diagnostico tiene que apuntar a quien escribio `Caja<int>`.
    std::size_t templateOrdinal = 0;
    const ast::AstNode* instantiationSyntax = nullptr;
    SourceRange instantiationRange;
    // NSC-LIMIT-007, segunda vuelta. CERO si esta especializacion es la unica respuesta posible a
    // su sitio de instanciacion; distinto de cero si salio de un conjunto de SOBRECARGAS que se
    // materializaron todas juntas porque comparten nombre y aridad y elegir entre ellas necesita
    // los tipos de los argumentos --que esta fase no tiene--.
    //
    // Importa para la validacion: en un grupo asi, que UNA candidata incumpla su constraint no es
    // un error del programa, porque la llamada puede estar resolviendo a otra. Solo lo es si
    // fallan TODAS. Sin este campo, `F<Valor>(...)` con dos `F<T>` declaradas se rechazaba por la
    // candidata equivocada.
    std::size_t overloadGroup = 0;
    // NSC305-G09. The demand-owned instantiation context (request site + packs + projections).
    InstantiationContext instantiationContext;
    // NSC305-G07. Explicit lifecycle state (see SpecializationState). A record is pushed as
    // Building and flips to Ready when the committed clone is attached; precompiled seed and lambda
    // instances are born Ready.
    SpecializationState state = SpecializationState::Unseen;
};

// Closed code that is already present in a referenced binary. Seeding the generic cache with
// these identities makes an equal consumer request bind to the producer implementation instead
// of cloning a second specialization into the consumer.
struct PrecompiledGenericSpecialization final
{
    GenericTemplateKind kind = GenericTemplateKind::Class;
    std::string templateKey;
    std::string specializationKey;
    std::string specializedName;
    std::vector<std::string> typeArguments;
    // True when the importing module's ordinary public-API source already declares the concrete
    // generated name.  A cross-module specialization used only through a published signature has
    // no such declaration: it must be materialized from its open template instead of merely
    // seeding the cache with a name that name lookup can never resolve.
    bool hasConcreteDeclaration = false;
};

struct GenericInstantiationReport final
{
    std::size_t materializedCount = 0;
    std::size_t cacheHitCount = 0;
    std::size_t maximumObservedDepth = 0;
    std::size_t maximumDepth = 0;
    std::size_t maximumCount = 0;
};

// NSC305-P01. Observability-only accumulator for the generic monomorphization phase.
//
// It records hierarchical sub-phase timings and aggregate counters so a `--time` /
// `--time-trace` run can attribute the `generics` phase to its internal steps and, with
// detail enabled, to individual templates. Recording is a no-op unless `enabled` is true;
// the per-template breakdown is recorded only when `detailed` is also true. This struct must
// never influence diagnostics, artifacts, control flow, ordering or scheduling: every
// instrumented site only appends to these fields or forwards a timing to `timingSink`.
struct GenericTelemetry final
{
    using TimingSink = std::function<void(std::string_view, std::chrono::nanoseconds)>;

    bool enabled = false;
    bool detailed = false;
    TimingSink timingSink;

    // Sub-phase wall times accumulated by name. A sub-phase may span more than one lexical
    // scope (for example `generics.template-index`); times for the same name are summed.
    std::unordered_map<std::string, std::uint64_t> subPhaseMicros;

    std::uint64_t ownUnits = 0;
    std::uint64_t templateSourceBytes = 0;
    std::uint64_t templatesCollected = 0;
    // Indexed by GenericTemplateKind (Class, Struct, Delegate, Method, Interface).
    std::uint64_t templatesByKind[5] = {};
    std::uint64_t resolutionQueries = 0;
    std::uint64_t candidatesExamined = 0;
    std::uint64_t demandsDiscovered = 0;
    std::uint64_t demandsDuplicated = 0;
    std::uint64_t demandsMaterialized = 0;
    std::uint64_t specializationHits = 0;
    std::uint64_t nodesCloned = 0;
    std::uint64_t typesCloned = 0;
    std::uint64_t maximumLogicalDepth = 0;
    // NSC305-G04. Scope-copy cardinality: how many `GenericLookupScope` deep copies were made and
    // the total container entries those copies moved. A large `scopeCopiedEntries` per copy is the
    // O(N^2) signal that motivates persistent scopes.
    std::uint64_t scopeCopies = 0;
    std::uint64_t scopeCopiedEntries = 0;

    struct TemplateDetail final
    {
        std::uint64_t materializations = 0;
        std::uint64_t exclusiveMicros = 0;
    };
    // Keyed by the template canonical key; populated only while `detailed` is enabled.
    std::unordered_map<std::string, TemplateDetail> templates;

    void recordSubPhase(std::string_view name, std::chrono::nanoseconds elapsed)
    {
        subPhaseMicros[std::string(name)] += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
        if (timingSink) timingSink(name, elapsed);
    }

    void noteResolutionQuery(std::size_t candidates)
    {
        if (!enabled) return;
        ++resolutionQueries;
        candidatesExamined += candidates;
    }

    void noteTemplate(std::size_t kindIndex)
    {
        if (!enabled) return;
        ++templatesCollected;
        if (kindIndex < 5U) ++templatesByKind[kindIndex];
    }

    void noteScopeCopy(std::size_t entries)
    {
        if (!enabled) return;
        ++scopeCopies;
        scopeCopiedEntries += entries;
    }

    void noteSpecializationHit()
    {
        if (enabled) ++specializationHits;
    }

    void noteMaterialization(
        std::string_view canonicalKey, std::chrono::nanoseconds elapsed)
    {
        if (!enabled) return;
        ++demandsMaterialized;
        if (detailed)
        {
            TemplateDetail& detail = templates[std::string(canonicalKey)];
            ++detail.materializations;
            detail.exclusiveMicros += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
        }
    }

    void noteDemand(bool duplicated)
    {
        if (!enabled) return;
        ++demandsDiscovered;
        if (duplicated) ++demandsDuplicated;
    }

    void noteNodesCloned()
    {
        if (enabled) ++nodesCloned;
    }

    void noteTypesCloned()
    {
        if (enabled) ++typesCloned;
    }

    class Scope final
    {
    public:
        Scope(GenericTelemetry& telemetry, std::string_view name)
            : telemetry_(&telemetry), name_(name),
              started_(telemetry.enabled ? std::chrono::steady_clock::now()
                                         : std::chrono::steady_clock::time_point{})
        {
        }
        ~Scope()
        {
            if (!telemetry_->enabled) return;
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started_);
            telemetry_->recordSubPhase(name_, elapsed);
        }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        GenericTelemetry* telemetry_;
        std::string_view name_;
        std::chrono::steady_clock::time_point started_;
    };

    // Deterministic, human-readable summary of counters and per-template detail. Implemented
    // in ProjectGenericModel.cpp to keep this header free of <sstream>.
    std::string renderSummary() const;
};

class ProjectGenericModel final
{
public:
    static constexpr std::size_t DefaultMaximumInstantiationDepth = 64;
    static constexpr std::size_t DefaultMaximumInstantiationCount = 16384;
    static constexpr std::size_t DefaultMaximumPackExpansionCount = 1024;

    explicit ProjectGenericModel(
        std::size_t maximumInstantiationDepth = DefaultMaximumInstantiationDepth,
        std::size_t maximumInstantiationCount = DefaultMaximumInstantiationCount) noexcept;

    bool build(
        const compilation::ProjectGraph& graph,
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        std::span<const PrecompiledGenericSpecialization> precompiledSpecializations,
        diagnostics::DiagnosticEngine& diagnostics);

    std::span<const GenericTemplateRecord> templates() const noexcept;
    std::span<const GenericSpecializationRecord> specializations() const noexcept;
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
    // NSC305-C02. Warm-build restore of one template record: on a cache hit, deserializes the
    // metadata, links the walked syntax node and pushes the TemplateState (skipping the record
    // construction). Returns false on miss or disabled cache.
    bool tryRestoreTemplateRecord(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        ast::AstNode* parent);

    // NSC305-C01. Persists the collected template records to the C1 cache (no-op when disabled).
    void storeTemplateRecords();
    // NSC305-C01. Restores the collected template records from the C1 cache and counts hits (no-op
    // when disabled). The AST subtree is still walked; this validates + reports the metadata hit.
    void restoreTemplateRecords();

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

    void collectTemplates(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        ast::AstNode* parent,
        std::string namespaceName,
        std::string ownerName,
        bool insideGenericType,
        diagnostics::DiagnosticEngine& diagnostics);

    bool mergeTraitImplementations(
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics);
    bool collectAssociatedItems(
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics);
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
            std::string name;
            const ast::TypeSyntax* type = nullptr;
            std::shared_ptr<const ValueBinding> parent;
        };
        std::shared_ptr<const ValueBinding> valueTypes;
        void addValueType(std::string name, const ast::TypeSyntax* type)
        {
            valueTypes = std::make_shared<ValueBinding>(
                ValueBinding{std::move(name), type, std::move(valueTypes)});
        }
        const ast::TypeSyntax* findValueType(std::string_view name) const noexcept
        {
            for (const ValueBinding* binding = valueTypes.get(); binding != nullptr;
                 binding = binding->parent.get())
            {
                if (binding->name == name) return binding->type;
            }
            return nullptr;
        }
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

    // NSC305-G09. The private output of a specialization worker (the committer merges it by the
    // reserved ordinal). The clone lives in the worker's arena and is linked on merge.
    struct SpecializationProduct final
    {
        std::size_t ordinal = 0;
        ast::AstNode* clone = nullptr;
        std::vector<GenericTypeDemand> childDemands;
        bool success = false;
    };

    // NSC305-G09. The private state of a specialization worker: its own arena (the clone lives here,
    // so workers do not race on the shared unit arena) and its collected child demands.
    struct SpecializationWorker final
    {
        std::unique_ptr<ast::AstArena> arena;
        std::vector<GenericTypeDemand> childDemands;
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

        void save(const ProjectGenericModel& model)
        {
            projectOrdinal = model.currentInstantiationProjectOrdinal_;
            namespaceName = model.currentInstantiationNamespace_;
            range = model.currentInstantiationRange_;
            overloadGroup = model.currentOverloadGroup_;
            packSubstitutions = model.activePackSubstitutions_;
            packError = model.packMaterializationError_;
            packErrorRange = model.packMaterializationErrorRange_;
            associatedProjections = model.activeAssociatedProjections_;
            associatedError = model.associatedProjectionError_;
            associatedErrorRange = model.associatedProjectionErrorRange_;
        }
        void restore(ProjectGenericModel& model) const
        {
            model.currentInstantiationProjectOrdinal_ = projectOrdinal;
            model.currentInstantiationNamespace_ = namespaceName;
            model.currentInstantiationRange_ = range;
            model.currentOverloadGroup_ = overloadGroup;
            model.activePackSubstitutions_ = packSubstitutions;
            model.packMaterializationError_ = packError;
            model.packMaterializationErrorRange_ = packErrorRange;
            model.activeAssociatedProjections_ = associatedProjections;
            model.associatedProjectionError_ = associatedError;
            model.associatedProjectionErrorRange_ = associatedErrorRange;
        }
    };

    bool specializeGenericLambdaCall(
        ProjectSyntaxUnit& unit,
        ast::AstNode* call,
        const GenericLookupScope::GenericLambda& lambda,
        const GenericLookupScope& scope,
        diagnostics::DiagnosticEngine& diagnostics);

    bool expandAliasesInNode(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        std::string namespaceName,
        GenericLookupScope scope,
        diagnostics::DiagnosticEngine& diagnostics);

    bool expandTypeAliases(
        ProjectSyntaxUnit& unit,
        ast::TypeSyntax* syntax,
        const GenericLookupScope& scope,
        diagnostics::DiagnosticEngine& diagnostics,
        std::vector<std::string>& expansionStack);

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

    ast::TypeSyntax* inferExpressionType(
        ProjectSyntaxUnit& unit,
        const ast::AstNode* expression,
        const GenericLookupScope& scope);

    bool inferTypeArguments(
        ProjectSyntaxUnit& unit,
        const ast::TypeSyntax* pattern,
        const ast::TypeSyntax* actual,
        const GenericTemplateRecord& record,
        const std::unordered_set<std::string>& parameters,
        std::unordered_map<std::string, ast::TypeSyntax*>& inferred);

    bool normalizeGenericArguments(
        const GenericTemplateRecord& record,
        std::span<ast::TypeSyntax* const> arguments,
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
    std::string ensureMethodSpecialization(
        TemplateState& state,
        std::span<ast::TypeSyntax* const> arguments,
        diagnostics::DiagnosticEngine& diagnostics,
        // ID 132. Cuando varias plantillas comparten nombre y aridad generica, todas se
        // especializan con ESTE nombre y se convierten en sobrecargas ordinarias. Vacio para el
        // caso normal, donde el nombre lo deriva la propia clave.
        std::string_view sharedName = {});

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
    // NSC305-G06. The non-substitution branch of cloneType: creates the shallow result, clones the
    // function return/error types and type arguments, and carries forward origin tags.
    ast::TypeSyntax* cloneTypeGeneric(
        ast::AstArena& arena,
        const ast::TypeSyntax* source,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
        std::size_t modifierLimit);
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

    std::string typeSpelling(const ast::TypeSyntax* syntax) const;
    std::string typeSpecializationIdentity(const ast::TypeSyntax* syntax) const;
    const DeclaredTypeIdentity* resolveDeclaredTypeIdentity(
        const ast::TypeSyntax* syntax,
        std::size_t projectOrdinal,
        std::string_view namespaceName,
        const GenericLookupScope& scope) const;
    static void appendTypeModifiers(std::string& identity, const ast::TypeSyntax* syntax);
    static std::string namespaceAppend(std::string_view prefix, std::string_view suffix);
    static std::string sanitize(std::string_view text);
    static std::string methodCalleeName(const ast::AstNode* call);
    static ast::AstNode* methodCalleeSyntax(ast::AstNode* call);
    void buildVisibility(const compilation::ProjectGraph& graph);
    bool projectVisible(std::size_t from, std::size_t target) const noexcept;
    bool templateVisible(std::size_t from, const GenericTemplateRecord& target) const noexcept;

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
    std::unordered_map<std::string, std::vector<TemplateState*>> typesByModuleQualified_;
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
        std::optional<std::size_t> request(const std::string& key) const
        {
            const auto found = byKey.find(key);
            return found == byKey.end() ? std::optional<std::size_t>{} : found->second;
        }
        void commit(const std::string& key, std::size_t ordinal)
        {
            byKey.emplace(key, ordinal);
        }
        void rollback(const std::string& key)
        {
            byKey.erase(key);
        }
    };
    SpecializationPlanner specializationPlanner_;
    std::unordered_map<const ast::AstNode*, std::size_t> specializationBySyntax_;
    // NSC305-G-series. specializedName -> specialization ordinal, built once at the end of build()
    // (after every materialization and rollback) so `specializationForTypeKey` is O(1). `emplace`
    // keeps the first entry for a duplicated name, matching the previous forward scan.
    std::unordered_map<std::string, std::size_t> specializationBySpecializedName_;
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
    // NSC305-G09. O(1) dedup of body-type demands by stableKey (the "deduplicate demands by
    // SpecializationKey" wave step); cleared after each wave is processed.
    std::unordered_set<std::string> bodyTypeDemandKeys_;
    // Available before the ordinary member/body model. Only an unambiguous common return for a
    // non-generic callable name is retained; this lets an unnamed `some Trait` identity provide
    // evidence to generic inference without writing semantic state onto CallExpression AST nodes.
    std::unordered_map<std::string, const ast::TypeSyntax*> ordinaryCallableReturnTypes_;
    std::unordered_map<const ast::AstNode*, std::vector<const ast::AstNode*>>
        traitImplExtensions_;
    std::unordered_map<std::string, const ast::TypeSyntax*> associatedTypes_;
    std::unordered_map<std::string, const ast::AstNode*> associatedConstants_;
    std::unordered_set<std::string> activeAssociatedProjections_;
    std::string associatedProjectionError_;
    SourceRange associatedProjectionErrorRange_{};
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
    // Active only while cloneNode/cloneType expands one specialization. A trailing type pack maps
    // to zero or more arguments; the map is saved/restored around recursive materialization.
    std::unordered_map<std::string, std::vector<const ast::TypeSyntax*>> activePackSubstitutions_;
    // cloneNode/cloneType cannot emit diagnostics directly. Pack indexing/folding records one
    // causal materialization error here; ensure*Specialization publishes it before attaching the
    // incomplete clone and removes the provisional cache entry.
    std::string packMaterializationError_;
    SourceRange packMaterializationErrorRange_{};
    // NSC305-G06. Recursion depth of the mutually-recursive cloneType/cloneNode pair, guarded so a
    // pathologically deep type/expression fails with a clean materialization error instead of
    // overflowing the process stack.
    std::size_t cloneDepth_ = 0;
};

const char* genericTemplateKindName(GenericTemplateKind kind) noexcept;
}
