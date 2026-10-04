#pragma once

#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/NamespaceSymbol.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
enum class NamedTypeDeclarationKind
{
    Class,
    Interface,
    Struct,
    Enum,
    Delegate
};

// NSC-LIMIT-009. `Private` y `Protected` existen para los tipos ANIDADOS: un tipo de namespace
// solo puede ser internal o public, y sigue siendo asi. El orden NO es una escala de mas a menos
// visible y no debe usarse como tal; las comparaciones del fichero son todas `== Public`, que es
// la unica pregunta que el cruce de modulos hace.
//
// `Protected` esta implementado, y su comprobacion vive en OTRA fase: responder «este consumidor
// deriva del contenedor» necesita el modelo de herencia, que se construye DESPUES de resolver
// nombres. Aqui el nombre se acepta y el binding guarda la cadena de tipos del sitio de uso;
// ProjectInheritanceModel::checkProtectedNestedTypeUses decide con la herencia ya construida.
enum class NamedTypeAccessibility
{
    Private,
    Internal,
    Package,
    Protected,
    Public
};

struct NamedTypeDeclarationPart final
{
    NamedTypeDeclarationKind kind = NamedTypeDeclarationKind::Struct;
    NamedTypeAccessibility accessibility = NamedTypeAccessibility::Internal;
    bool hasExplicitAccessibility = false;
    // NSC-LIMIT-009. Declarado dentro de otro tipo. Se guarda aunque la identidad ya lo diga
    // porque el agregador de partes solo entiende internal/public: para un anidado hay que
    // saltarlo y conservar la accesibilidad que se calculo al predeclarar.
    bool isNested = false;
    bool isPartial = false;
    bool isAbstract = false;
    bool isSealed = false;
    bool isStatic = false;
    bool isPrecompiled = false;
    std::size_t projectOrdinal = 0;
    std::size_t sourceOrdinal = 0;
    FileId fileId;
    SourceRange range;
    const ast::AstNode* syntax = nullptr;
    std::string sourcePath;
};

struct NamedTypeDeclaration final
{
    compilation::TypeIdentity identity;
    NamedTypeDeclarationKind kind = NamedTypeDeclarationKind::Struct;
    NamedTypeAccessibility accessibility = NamedTypeAccessibility::Internal;
    // NSC-LIMIT-009. El tipo que lo contiene, si lo hay. Es un ORDINAL y no un nombre porque la
    // pregunta que se le hace --«estoy dentro de este contenedor?»-- se responde comparando
    // ordinales con la cadena que el entorno de lookup lleva, sin volver a resolver nada.
    std::optional<std::size_t> containingTypeOrdinal;
    bool isPartial = false;
    bool isAbstract = false;
    bool isSealed = false;
    bool isStatic = false;
    bool isPrecompiled = false;
    std::size_t projectOrdinal = 0;
    std::size_t sourceOrdinal = 0;
    FileId fileId;
    SourceRange range;
    const ast::AstNode* syntax = nullptr;
    std::vector<NamedTypeDeclarationPart> parts;
};

enum class NamespaceFunctionAccessibility
{
    Internal,
    Public
};

struct NamespaceFunctionDeclaration final
{
    NamespaceFunctionAccessibility accessibility = NamespaceFunctionAccessibility::Internal;
    std::size_t projectOrdinal = 0;
    std::size_t sourceOrdinal = 0;
    FileId fileId;
    SourceRange range;
    const ast::AstNode* syntax = nullptr;
    std::string sourcePath;
    std::string moduleName;
    std::string namespaceName;
    std::string name;
    bool hasBody = false;
    bool isPrecompiled = false;
};

struct NamespaceStateDeclaration final
{
    NamespaceFunctionAccessibility accessibility = NamespaceFunctionAccessibility::Internal;
    std::size_t projectOrdinal = 0;
    std::size_t sourceOrdinal = 0;
    FileId fileId;
    SourceRange range;
    const ast::AstNode* syntax = nullptr;
    std::string sourcePath;
    std::string moduleName;
    std::string namespaceName;
    std::string name;
    bool isConst = false;
    bool isPrecompiled = false;
};
enum class TypeNameBindingKind
{
    Builtin,
    Resolved,
    Unresolved,
    Ambiguous
};

struct TypeNameBinding final
{
    const ast::TypeSyntax* syntax = nullptr;
    TypeNameBindingKind kind = TypeNameBindingKind::Unresolved;
    std::size_t projectOrdinal = 0;
    FileId fileId;
    std::string namespaceName;
    std::string writtenName;
    std::optional<std::size_t> targetTypeOrdinal;
    std::vector<std::size_t> candidateTypeOrdinals;
    // NSC-LIMIT-009. Los tipos dentro de los que se escribio ESTE nombre, del mas exterior al mas
    // interior. La resolucion de nombres los usa para ver los anidados `private` del contenedor;
    // se guardan aqui porque `protected` no se puede responder todavia --hace falta saber si el
    // sitio deriva del contenedor, y la herencia se construye una fase mas tarde-- y quien la
    // responda necesita saber desde donde se escribio.
    std::vector<std::size_t> enclosingTypeOrdinals;
};

struct ExpressionTypeNameBinding final
{
    const ast::AstNode* expression = nullptr;
    std::string writtenName;
    std::vector<std::size_t> candidateTypeOrdinals;
    // NSC-LIMIT-009, segunda vuelta. LA MISMA CADENA QUE GUARDA TypeNameBinding, y por la misma
    // razon: `Base.Reservada.Codigo()` nombra un tipo SIN escribir un TypeSyntax --el receptor es
    // una expresion-- de modo que un anidado `protected` se alcanzaba por este camino sin que
    // nadie comprobara nada. Ahora los dos tipos de binding llevan el contexto lexico del sitio y
    // la comprobacion diferida recorre los dos.
    std::vector<std::size_t> enclosingTypeOrdinals;
};

class ModuleDeclarationSpace final
{
public:
    ModuleDeclarationSpace(std::size_t projectOrdinal, std::string moduleName);

    std::size_t projectOrdinal() const noexcept;
    const std::string& moduleName() const noexcept;
    GlobalNamespace& globalNamespace() noexcept;
    const GlobalNamespace& globalNamespace() const noexcept;

    void addTypeOrdinal(std::size_t ordinal);
    std::span<const std::size_t> typeOrdinals() const noexcept;

    void setVisibleProjectOrdinals(std::vector<std::size_t> ordinals);
    std::span<const std::size_t> visibleProjectOrdinals() const noexcept;

private:
    std::size_t projectOrdinal_ = 0;
    std::string moduleName_;
    GlobalNamespace globalNamespace_;
    std::vector<std::size_t> typeOrdinals_;
    std::vector<std::size_t> visibleProjectOrdinals_;
};

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

    const NamedTypeDeclaration* resolvedDeclaration(const ast::TypeSyntax* syntax) const noexcept;
    std::optional<std::size_t> resolvedTypeOrdinal(const ast::TypeSyntax* syntax) const noexcept;
    const ExpressionTypeNameBinding* expressionTypeBinding(
        const ast::AstNode* expression) const noexcept;
    std::span<const ExpressionTypeNameBinding> expressionTypeBindings() const noexcept;
    bool isBuilt() const noexcept;

private:
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
    void finalizePredeclarations(diagnostics::DiagnosticEngine& diagnostics);
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
    void bindNode(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        const std::vector<std::string>& namespaceSegments,
        const LookupEnvironment& environment,
        diagnostics::DiagnosticEngine& diagnostics,
        UnitBindingIndex& index);
    void bindTypeSyntax(
        const ProjectSyntaxUnit& unit,
        const ast::TypeSyntax* syntax,
        const std::vector<std::string>& namespaceSegments,
        const LookupEnvironment& environment,
        diagnostics::DiagnosticEngine& diagnostics,
        UnitBindingIndex& index);
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
    std::vector<NamespaceFunctionDeclaration> namespaceFunctions_;
    std::vector<NamespaceStateDeclaration> namespaceStates_;
    std::vector<TypeNameBinding> bindings_;
    std::vector<std::unordered_map<std::string, std::size_t>> typesByModule_;
    // NSC-LIMIT-009. `ns.Outer.Item` -> ordinal. Solo tipos anidados; nunca sustituye a
    // typesByModule_, que sigue siendo la clave canonica con `+`.
    std::vector<std::unordered_map<std::string, std::size_t>> nestedSourceNamesByModule_;
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
