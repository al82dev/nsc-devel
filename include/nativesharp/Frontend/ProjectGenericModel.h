#pragma once

#include "nativesharp/Frontend/QualifiedNameLadder.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Language/GenericSemantics.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
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

    // Explicit generic uses that could not be resolved while inspecting the template. They are
    // harmless in an unreachable private template, but make a public reusable closure invalid.
    std::vector<std::string> unresolvedDependencies;
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

class ProjectGenericModel final
{
public:
    static constexpr std::size_t DefaultMaximumInstantiationDepth = 64;
    static constexpr std::size_t DefaultMaximumInstantiationCount = 16384;
    static constexpr std::size_t DefaultMaximumPackExpansionCount = 1024;

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
    GenericInstantiationReport instantiationReport() const noexcept;
    bool isBuilt() const noexcept;

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
        const std::string& namespaceName,
        const std::string& ownerName,
        bool insideGenericType,
        diagnostics::DiagnosticEngine& diagnostics);

    void discoverTemplateDependencies(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        const std::string& namespaceName,
        const std::string& ownerName,
        const GenericLookupScope& scope);
    void collectTemplateDependencies(
        TemplateState& root,
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        const std::string& namespaceName,
        const std::string& ownerName,
        const GenericLookupScope& scope,
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
        std::vector<std::string> importedNamespaces;
        std::unordered_map<std::string, std::string> aliases;
        // Item 276. Type aliases have to disappear before generic templates are registered:
        // otherwise `Alias<int>` is mistaken for a template identity of its own.  The declaration
        // carries both the target syntax and its optional type-parameter list.
        std::unordered_map<std::string, const ast::AstNode*> typeAliases;
        std::unordered_set<std::string> typeParameters;
        // Item 277. Types written on parameters, fields, and preceding locals are sufficient for
        // local generic inference without running a second semantic pipeline before Stage 44.
        std::unordered_map<std::string, const ast::TypeSyntax*> valueTypes;
        // Return type before closed generic spellings are rewritten. It provides expected-type
        // evidence to a direct generic call in a return statement.
        const ast::TypeSyntax* expectedReturnType = nullptr;
    };

    bool expandAliasesInNode(
        ProjectSyntaxUnit& unit,
        ast::AstNode* node,
        const std::string& namespaceName,
        const GenericLookupScope& scope,
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
        const std::string& namespaceName,
        const std::string& ownerName,
        const GenericLookupScope& scope,
        bool insideTemplate,
        diagnostics::DiagnosticEngine& diagnostics);

    bool processTypeSyntax(
        ProjectSyntaxUnit& unit,
        ast::TypeSyntax* syntax,
        const std::string& namespaceName,
        const GenericLookupScope& scope,
        diagnostics::DiagnosticEngine& diagnostics);

    bool processGenericCall(
        ProjectSyntaxUnit& unit,
        ast::AstNode* call,
        const std::string& namespaceName,
        const std::string& ownerName,
        const GenericLookupScope& scope,
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

    TemplateState* resolveTypeTemplate(
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
        std::size_t argumentCount);

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
        std::size_t argumentCount);

    std::string ensureTypeSpecialization(
        TemplateState& state,
        std::span<ast::TypeSyntax* const> arguments,
        diagnostics::DiagnosticEngine& diagnostics);
    std::string ensureMethodSpecialization(
        TemplateState& state,
        std::span<ast::TypeSyntax* const> arguments,
        diagnostics::DiagnosticEngine& diagnostics,
        // ID 132. Cuando varias plantillas comparten nombre y aridad generica, todas se
        // especializan con ESTE nombre y se convierten en sobrecargas ordinarias. Vacio para el
        // caso normal, donde el nombre lo deriva la propia clave.
        std::string_view sharedName = {});

    ast::TypeSyntax* cloneType(
        ast::AstArena& arena,
        const ast::TypeSyntax* source,
        const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions);

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
    std::vector<DeclaredTypeIdentity> declaredTypeIdentities_;
    std::vector<GenericTemplateRecord> templateRecords_;
    std::vector<GenericSpecializationRecord> specializations_;
    std::unordered_map<std::string, std::size_t> specializationByKey_;
    std::unordered_map<const ast::AstNode*, std::size_t> specializationBySyntax_;
    std::unordered_map<const ast::TypeSyntax*, std::string> resolvedTypeIdentityBySyntax_;
    std::unordered_map<const ast::TypeSyntax*, DeclaredTypeOrigin> resolvedTypeOriginBySyntax_;
    std::unordered_map<const ast::AstNode*, ast::TypeSyntax*> inferredCallReturnTypes_;
    std::vector<std::vector<std::size_t>> visibleProjects_;
    std::vector<std::string> projectModuleNames_;
    // Available before name binding because generic materialization itself runs before name
    // binding. Values use the enum's canonical declaration identity plus normalized underlying
    // bits, so aliases with the same value share one specialization.
    std::unordered_map<std::string, std::string> constEnumTypeBySpelling_;
    std::unordered_map<std::string, std::pair<std::string, std::string>> constEnumValueBySpelling_;
    bool built_ = false;

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
    language::InstantiationBudget instantiationBudget_{
        DefaultMaximumInstantiationDepth, DefaultMaximumInstantiationCount};
    std::size_t instantiationCacheHitCount_ = 0;
    // Active only while cloneNode/cloneType expands one specialization. A trailing type pack maps
    // to zero or more arguments; the map is saved/restored around recursive materialization.
    std::unordered_map<std::string, std::vector<const ast::TypeSyntax*>> activePackSubstitutions_;
    // cloneNode/cloneType cannot emit diagnostics directly. Pack indexing/folding records one
    // causal materialization error here; ensure*Specialization publishes it before attaching the
    // incomplete clone and removes the provisional cache entry.
    std::string packMaterializationError_;
    SourceRange packMaterializationErrorRange_{};
};

const char* genericTemplateKindName(GenericTemplateKind kind) noexcept;
}
