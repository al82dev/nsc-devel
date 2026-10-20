#pragma once

#include "nativesharp/Frontend/NamedTypeDeclarationKind.h"
#include "nativesharp/Frontend/NamedTypeAccessibility.h"
#include "nativesharp/Frontend/NamedTypeDeclarationPart.h"
#include "nativesharp/Frontend/NamedTypeDeclaration.h"
#include "nativesharp/Frontend/NamespaceFunctionAccessibility.h"
#include "nativesharp/Frontend/NamespaceFunctionDeclaration.h"
#include "nativesharp/Frontend/NamespaceStateDeclaration.h"
#include "nativesharp/Frontend/PendingReexport.h"
#include "nativesharp/Frontend/TypeNameBindingKind.h"
#include "nativesharp/Frontend/TypeNameBinding.h"
#include "nativesharp/Frontend/ExpressionTypeNameBinding.h"
#include "nativesharp/Frontend/ModuleDeclarationSpace.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/NamespaceSymbol.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"
#include "nativesharp/Frontend/QualifiedNameTier.h"

#include <cstddef>
#include <cstdint>
#include <memory>
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

namespace nativesharp::compilation
{
class PartialTypeAggregator;
}

namespace nativesharp::driver
{
class CompilationWorkerPool;
}

namespace nativesharp::frontend
{
struct ProjectOopHirModelTestAccess;


// NSC-LIMIT-009. `Private` y `Protected` existen para los tipos ANIDADOS: un tipo de namespace
// solo puede ser internal o public, y sigue siendo asi. El orden NO es una escala de mas a menos
// visible y no debe usarse como tal; las comparaciones del fichero son todas `== Public`, que es
// la unica pregunta que el cruce de modulos hace.
//
// `Protected` esta implementado, y su comprobacion vive en OTRA fase: responder «este consumidor
// deriva del contenedor» necesita el modelo de herencia, que se construye DESPUES de resolver
// nombres. Aqui el nombre se acepta y el binding guarda la cadena de tipos del sitio de uso;
// ProjectInheritanceModel::checkProtectedNestedTypeUses decide con la herencia ya construida.









class ProjectNameLookup final
{
public:
    bool build(
        const compilation::ProjectGraph& graph,
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics,
        std::size_t workerCount = 1);

    std::span<const std::unique_ptr<ModuleDeclarationSpace>> modules() const noexcept;
    std::span<const NamedTypeDeclaration> namedTypes() const noexcept;
    std::span<const NamespaceFunctionDeclaration> namespaceFunctions() const noexcept;
    std::span<const NamespaceStateDeclaration> namespaceStates() const noexcept;
    std::span<const TypeNameBinding> bindings() const noexcept;
    // NS304-T08-F05: trait-impl parts whose target is an external type (the module owns the trait,
    // not the type). The member model registers their methods on the external target.
    std::span<const NamedTypeDeclarationPart> externalTraitImplParts() const noexcept;

    const NamedTypeDeclaration* resolvedDeclaration(const ast::TypeSyntax* syntax) const noexcept;
    std::optional<std::size_t> resolvedTypeOrdinal(const ast::TypeSyntax* syntax) const noexcept;
    const ExpressionTypeNameBinding* expressionTypeBinding(
        const ast::AstNode* expression) const noexcept;
    std::span<const ExpressionTypeNameBinding> expressionTypeBindings() const noexcept;
    bool isBuilt() const noexcept;

private:
    friend struct ProjectOopHirModelTestAccess;

    enum class Stage : std::uint8_t;
    struct Frame;
    struct LookupEnvironment;

    struct PendingTypeDeclaration final
    {
        compilation::TypeIdentity identity;
        NamedTypeDeclarationPart part;
    };

    struct UnitDeclarationIndex final
    {
        std::vector<std::vector<std::string>> namespaces;
        std::vector<PendingTypeDeclaration> pendingTypes;
        std::vector<NamespaceFunctionDeclaration> namespaceFunctions;
        std::vector<NamespaceStateDeclaration> namespaceStates;
    };

    struct UnitBindingIndex final
    {
        std::vector<TypeNameBinding> bindings;
        std::unordered_map<const ast::TypeSyntax*, std::size_t> bindingBySyntax;
        std::vector<ExpressionTypeNameBinding> expressionTypeBindings;
        std::unordered_map<const ast::AstNode*, std::size_t> expressionTypeBindingBySyntax;
    };

    void initializeModules(const compilation::ProjectGraph& graph);
    void collectDeclarationIndexes(
        const compilation::ProjectGraph& graph,
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        driver::CompilationWorkerPool& workers);
    static bool namespaceFunctionLess(
        const NamespaceFunctionDeclaration& left,
        const NamespaceFunctionDeclaration& right);
    static bool namespaceStateLess(
        const NamespaceStateDeclaration& left,
        const NamespaceStateDeclaration& right);
    void finalizeNamespaceDeclarations();
    void mergeBindingIndexes(std::vector<UnitBindingIndex>& bindingIndexes);
    void collectBindingIndexes(
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics,
        driver::CompilationWorkerPool& workers);
    void buildVisibility(const compilation::ProjectGraph& graph);
    void indexUnitDeclarations(
        const ProjectSyntaxUnit& unit,
        const std::string& sourcePath,
        UnitDeclarationIndex& index) const;
    void predeclareNode(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        const std::vector<std::string>& namespaceSegments,
        const std::vector<compilation::TypeNameSegment>& containingTypes,
        const std::string& sourcePath,
        UnitDeclarationIndex& index) const;
    void predeclareNamespace(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        const std::vector<std::string>& namespaceSegments,
        const std::vector<compilation::TypeNameSegment>& containingTypes,
        const std::string& sourcePath,
        UnitDeclarationIndex& index) const;
    static void predeclareNamespaceFunction(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        const std::vector<std::string>& namespaceSegments,
        const std::string& sourcePath,
        UnitDeclarationIndex& index);
    static void predeclareNamespaceState(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        const std::vector<std::string>& namespaceSegments,
        const std::string& sourcePath,
        UnitDeclarationIndex& index);
    void predeclareExtension(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        const std::vector<std::string>& namespaceSegments,
        const std::vector<compilation::TypeNameSegment>& containingTypes,
        const std::string& sourcePath,
        UnitDeclarationIndex& index) const;
    static NamedTypeAccessibility predeclaredTypeAccessibility(
        const ast::AstNode* node,
        bool isNested);
    static NamedTypeDeclarationPart predeclaredTypePart(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        const std::string& sourcePath,
        bool isNested,
        NamedTypeAccessibility accessibility);
    void predeclareNestedTypes(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        const std::vector<std::string>& namespaceSegments,
        const std::vector<compilation::TypeNameSegment>& containingTypes,
        const std::string& sourcePath,
        UnitDeclarationIndex& index) const;
    void predeclareNamedType(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        const std::vector<std::string>& namespaceSegments,
        const std::vector<compilation::TypeNameSegment>& containingTypes,
        const std::string& sourcePath,
        UnitDeclarationIndex& index) const;
    void predeclareCompilationUnit(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        const std::vector<std::string>& namespaceSegments,
        const std::vector<compilation::TypeNameSegment>& containingTypes,
        const std::string& sourcePath,
        UnitDeclarationIndex& index) const;
    static bool pendingTypeLess(
        const PendingTypeDeclaration& left,
        const PendingTypeDeclaration& right);
    std::size_t pendingTypeGroupEnd(std::size_t groupBegin) const;
    bool collectPendingTypeGroup(
        std::size_t groupBegin,
        std::size_t groupEnd,
        compilation::PartialTypeAggregator& aggregator,
        std::string& aggregationError,
        std::optional<std::size_t>& primaryIndex) const;
    bool moduleOwnsTraitImplementation(
        std::size_t groupBegin,
        std::size_t groupEnd) const;
    void finalizeOrphanTraitImplementationGroup(
        std::size_t groupBegin,
        std::size_t groupEnd,
        diagnostics::DiagnosticEngine& diagnostics);
    void validateTraitImplementationTarget(
        std::size_t groupBegin,
        std::size_t groupEnd,
        const PendingTypeDeclaration& primary,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void reportTypeAggregationFailure(
        std::size_t groupBegin,
        std::size_t groupEnd,
        std::size_t primaryIndex,
        const std::string& aggregationError,
        diagnostics::DiagnosticEngine& diagnostics) const;
    NamedTypeAccessibility aggregatedTypeAccessibility(
        std::size_t groupBegin,
        std::size_t groupEnd,
        const PendingTypeDeclaration& primary,
        const compilation::PartialTypeAggregator& aggregator,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void applyAggregatedTypeProperties(
        std::size_t groupBegin,
        std::size_t groupEnd,
        const PendingTypeDeclaration& primary,
        const compilation::PartialTypeAggregator& aggregator,
        diagnostics::DiagnosticEngine& diagnostics,
        NamedTypeAccessibility& accessibility,
        bool& isPartial,
        bool& isAbstract,
        bool& isSealed,
        bool& isStatic) const;
    std::vector<NamedTypeDeclarationPart> pendingDeclarationParts(
        std::size_t groupBegin,
        std::size_t groupEnd,
        const PendingTypeDeclaration& primary,
        bool aggregated) const;
    std::size_t appendNamedType(
        const PendingTypeDeclaration& primary,
        NamedTypeAccessibility accessibility,
        bool isPartial,
        bool isAbstract,
        bool isSealed,
        bool isStatic,
        std::vector<NamedTypeDeclarationPart> declarationParts);
    void registerNestedSourceName(
        const PendingTypeDeclaration& primary,
        std::size_t typeOrdinal);
    void registerTypeDeclarationIndexes(
        const PendingTypeDeclaration& primary,
        std::size_t typeOrdinal);
    void finalizePredeclarationGroup(
        std::size_t groupBegin,
        std::size_t groupEnd,
        diagnostics::DiagnosticEngine& diagnostics);
    void finalizeContainingTypeOrdinals();
    void finalizePredeclarations(diagnostics::DiagnosticEngine& diagnostics);
    void finalizeReexports(
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics);
    void collectReexports(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        const std::string& namespaceName,
        std::vector<PendingReexport>& pending) const;
    void appendReexportCandidate(
        std::vector<std::size_t>& candidates,
        std::size_t moduleOrdinal,
        const std::string& targetName) const;
    bool isReexportPubliclyExportable(std::size_t target) const;
    void finalizeReexport(
        const PendingReexport& item,
        diagnostics::DiagnosticEngine& diagnostics);
    static bool pendingReexportLess(
        const PendingReexport& left,
        const PendingReexport& right);
    void bindUnit(
        const ProjectSyntaxUnit& unit,
        diagnostics::DiagnosticEngine& diagnostics,
        UnitBindingIndex& index);
    void bindScope(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* scopeNode,
        const std::vector<std::string>& namespaceSegments,
        const LookupEnvironment& inheritedEnvironment,
        diagnostics::DiagnosticEngine& diagnostics,
        UnitBindingIndex& index);
    void bindUsingDirectives(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* scopeNode,
        const std::vector<std::string>& namespaceSegments,
        LookupEnvironment& environment,
        diagnostics::DiagnosticEngine& diagnostics);
    void bindUsingNamespaceDirective(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* directive,
        const std::vector<std::string>& namespaceSegments,
        LookupEnvironment& environment,
        diagnostics::DiagnosticEngine& diagnostics);
    void bindUsingAliasDirective(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* directive,
        const std::vector<std::string>& namespaceSegments,
        LookupEnvironment& environment,
        std::unordered_set<std::string>& localAliases,
        diagnostics::DiagnosticEngine& diagnostics);
    void bindNode(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        const std::vector<std::string>& namespaceSegments,
        const LookupEnvironment& environment,
        diagnostics::DiagnosticEngine& diagnostics,
        UnitBindingIndex& index);
    bool bindGenericTemplateConstraints(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        const std::vector<std::string>& namespaceSegments,
        const LookupEnvironment& environment,
        diagnostics::DiagnosticEngine& diagnostics,
        UnitBindingIndex& index);
    void bindSpecialNodeTypeReferences(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        const std::vector<std::string>& namespaceSegments,
        const LookupEnvironment& environment,
        diagnostics::DiagnosticEngine& diagnostics,
        UnitBindingIndex& index);
    void bindConstraintTypeReferences(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        const std::vector<std::string>& namespaceSegments,
        const LookupEnvironment& environment,
        diagnostics::DiagnosticEngine& diagnostics,
        UnitBindingIndex& index);
    bool bindNamespaceDeclaration(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        const std::vector<std::string>& namespaceSegments,
        const LookupEnvironment& environment,
        diagnostics::DiagnosticEngine& diagnostics,
        UnitBindingIndex& index);
    std::optional<std::size_t> enclosingTypeOrdinal(const ast::AstNode* node) const;
    void bindTypeSyntax(
        const ProjectSyntaxUnit& unit,
        const ast::TypeSyntax* syntax,
        const std::vector<std::string>& namespaceSegments,
        const LookupEnvironment& environment,
        diagnostics::DiagnosticEngine& diagnostics,
        UnitBindingIndex& index);
    static void copyFunctionTypeAlias(
        ast::TypeSyntax& destination,
        const ast::TypeSyntax& source);
    static void expandTypeAlias(
        const ast::TypeSyntax* syntax,
        const LookupEnvironment& environment,
        diagnostics::DiagnosticEngine& diagnostics);
    void bindChildTypeSyntaxes(
        const ProjectSyntaxUnit& unit,
        const ast::TypeSyntax* syntax,
        const std::vector<std::string>& namespaceSegments,
        const LookupEnvironment& environment,
        diagnostics::DiagnosticEngine& diagnostics,
        UnitBindingIndex& index);
    static std::vector<std::string> effectiveTypeNamespace(
        const ast::TypeSyntax* syntax,
        const std::vector<std::string>& namespaceSegments);
    static bool isBuiltinTypeSurface(
        const ast::TypeSyntax* syntax,
        bool portableSimd);
    static void validatePortableSimdType(
        const ast::TypeSyntax* syntax,
        diagnostics::DiagnosticEngine& diagnostics);
    static void validateBuiltinTypeBinding(
        const ast::TypeSyntax* syntax,
        bool portableSimd,
        TypeNameBinding& binding,
        diagnostics::DiagnosticEngine& diagnostics);
    void appendObjectTypeRoot(TypeNameBinding& binding, const ast::TypeSyntax* syntax) const;
    void findReferencedInaccessibleType(
        std::size_t projectOrdinal,
        const std::string& wanted,
        std::string& internalMatch,
        std::optional<std::size_t>& inaccessibleOrdinal) const;
    void reportInaccessibleNestedType(
        const ast::TypeSyntax* syntax,
        std::size_t inaccessibleOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void reportInaccessibleReferencedType(
        const ProjectSyntaxUnit& unit,
        const ast::TypeSyntax* syntax,
        const std::string& internalMatch,
        std::optional<std::size_t> inaccessibleOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void reportUnresolvedType(
        const ProjectSyntaxUnit& unit,
        const ast::TypeSyntax* syntax,
        const std::vector<std::string>& namespaceSegments,
        std::size_t projectOrdinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void resolveTypeNameBinding(
        const ProjectSyntaxUnit& unit,
        const ast::TypeSyntax* syntax,
        const std::vector<std::string>& namespaceSegments,
        const LookupEnvironment& environment,
        TypeNameBinding& binding,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void bindExpressionTypeReceiver(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* expression,
        const std::vector<std::string>& namespaceSegments,
        const LookupEnvironment& environment,
        UnitBindingIndex& index);

    std::vector<std::string> enclosingTypeSpellings(const LookupEnvironment& environment) const;
    std::vector<std::size_t> resolveTypeCandidates(
        std::size_t projectOrdinal,
        const std::vector<std::string>& namespaceSegments,
        const LookupEnvironment& environment,
        const std::string& writtenName) const;
    std::vector<std::size_t> resolveModuleQualifiedTypeCandidates(
        std::size_t projectOrdinal,
        const std::string& writtenName,
        std::size_t moduleSeparator) const;
    void appendImportedNamespaceTypeCandidates(
        std::vector<std::size_t>& found,
        std::size_t projectOrdinal,
        const LookupEnvironment& environment,
        const std::string& candidate,
        bool simpleName) const;
    void appendReferencedModuleTypeCandidates(
        std::vector<std::size_t>& found,
        std::size_t projectOrdinal,
        const std::string& candidate,
        bool simpleName) const;
    void appendQualifiedTypeCandidate(
        std::vector<std::size_t>& found,
        std::size_t projectOrdinal,
        const LookupEnvironment& environment,
        const std::string& candidate,
        QualifiedNameTier tier,
        bool simpleName) const;
    std::vector<std::size_t> resolveExpressionTypeCandidates(
        std::size_t projectOrdinal,
        const std::vector<std::string>& namespaceSegments,
        const LookupEnvironment& environment,
        const std::string& writtenName) const;
    std::vector<std::size_t> findExactTypeCandidates(
        std::size_t projectOrdinal,
        const std::string& fullyQualifiedName,
        bool currentModuleFirst) const;
    // NSC-LIMIT-009. El nombre FUENTE de un tipo, con punto, en un modulo concreto:
    // `ns.Outer.Item`. Consulta primero el mapa canonico --donde la clave lleva `+`-- y despues
    // el indice de nombres fuente. Los dos mapas existen a proposito: el canonico es la
    // identidad, y sustituirlo por la forma con puntos volveria a hacer indistinguibles
    // `ns.Outer.Item` anidado y `ns.Outer.Item` de namespace.
    std::optional<std::size_t> findTypeInModule(
        std::size_t moduleOrdinal,
        const std::string& dottedName) const;
    // NSC-LIMIT-009. Un tipo cruza a otro modulo solo si el y TODOS sus contenedores son public.
    bool typeIsVisibleTo(std::size_t accessingProjectOrdinal, std::size_t typeOrdinal) const;
    // NSC-LIMIT-009. El anidado que un nombre habria nombrado si la accesibilidad lo permitiera.
    // Existe solo para el diagnostico: decir «no existe» de un tipo que esta escrito tres lineas
    // mas arriba manda al lector a buscar una errata que no hay.
    std::optional<std::size_t> findInaccessibleNestedType(
        std::size_t projectOrdinal,
        const std::vector<std::string>& namespaceSegments,
        const std::string& writtenName) const;
    // NSC-LIMIT-009. Un anidado `private` solo se ve desde su contenedor o desde otro tipo
    // anidado en el. Un anidado `public`/`internal` sigue la regla de modulo de siempre.
    bool nestedTypeIsVisible(
        std::size_t typeOrdinal,
        const LookupEnvironment& environment) const;
    // NSC-LIMIT-009. Los anidados directos de los contenedores lexicos, de dentro hacia fuera.
    // Es el PRIMER escalon de la busqueda dentro de un tipo, antes del namespace.
    bool namespaceExistsVisible(std::size_t projectOrdinal, const std::string& namespaceName) const;
    std::optional<std::string> resolveNamespaceTarget(
        std::size_t projectOrdinal,
        const std::vector<std::string>& namespaceSegments,
        const std::string& target) const;
    std::vector<std::unique_ptr<ModuleDeclarationSpace>> modules_;
    std::vector<PendingTypeDeclaration> pendingTypes_;
    std::vector<NamedTypeDeclaration> namedTypes_;
    std::vector<NamedTypeDeclarationPart> externalTraitImplParts_;
    std::vector<NamespaceFunctionDeclaration> namespaceFunctions_;
    std::vector<NamespaceStateDeclaration> namespaceStates_;
    std::vector<TypeNameBinding> bindings_;
    std::vector<std::unordered_map<std::string, std::size_t>> typesByModule_;
    // NSC-LIMIT-009. `ns.Outer.Item` -> ordinal. Solo tipos anidados; nunca sustituye a
    // typesByModule_, que sigue siendo la clave canonica con `+`.
    std::vector<std::unordered_map<std::string, std::size_t>> nestedSourceNamesByModule_;
    std::vector<std::unordered_map<std::string, std::size_t>> reexportedTypesByModule_;
    // NSC-LIMIT-009. El nodo de sintaxis de cada parte -> su ordinal de tipo. Es lo que permite a
    // `bindNode` saber en que tipo esta entrando sin reconstruir la identidad a mano.
    std::unordered_map<const ast::AstNode*, std::size_t> typeOrdinalBySyntax_;
    std::vector<std::string> packageByProject_;
    std::vector<std::vector<std::string>> friendsByProject_;
    std::unordered_map<const ast::TypeSyntax*, std::size_t> bindingBySyntax_;
    std::vector<ExpressionTypeNameBinding> expressionTypeBindings_;
    std::unordered_map<const ast::AstNode*, std::size_t> expressionTypeBindingBySyntax_;
    bool built_ = false;
};

const char* namedTypeDeclarationKindName(NamedTypeDeclarationKind kind) noexcept;
const char* namedTypeAccessibilityName(NamedTypeAccessibility accessibility) noexcept;
const char* typeNameBindingKindName(TypeNameBindingKind kind) noexcept;
}
