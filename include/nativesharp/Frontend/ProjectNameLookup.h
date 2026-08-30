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

enum class NamedTypeAccessibility
{
    Internal,
    Public
};

struct NamedTypeDeclarationPart final
{
    NamedTypeDeclarationKind kind = NamedTypeDeclarationKind::Struct;
    NamedTypeAccessibility accessibility = NamedTypeAccessibility::Internal;
    bool hasExplicitAccessibility = false;
    bool isPartial = false;
    bool isAbstract = false;
    bool isSealed = false;
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
    bool isPartial = false;
    bool isAbstract = false;
    bool isSealed = false;
    std::size_t projectOrdinal = 0;
    std::size_t sourceOrdinal = 0;
    FileId fileId;
    SourceRange range;
    const ast::AstNode* syntax = nullptr;
    std::vector<NamedTypeDeclarationPart> parts;
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
        diagnostics::DiagnosticEngine& diagnostics);

    std::span<const std::unique_ptr<ModuleDeclarationSpace>> modules() const noexcept;
    std::span<const NamedTypeDeclaration> namedTypes() const noexcept;
    std::span<const TypeNameBinding> bindings() const noexcept;

    const NamedTypeDeclaration* resolvedDeclaration(const ast::TypeSyntax* syntax) const noexcept;
    std::optional<std::size_t> resolvedTypeOrdinal(const ast::TypeSyntax* syntax) const noexcept;
    bool isBuilt() const noexcept;

private:
    struct LookupEnvironment;

    struct PendingTypeDeclaration final
    {
        compilation::TypeIdentity identity;
        NamedTypeDeclarationPart part;
    };

    void buildVisibility(const compilation::ProjectGraph& graph);
    void predeclareUnit(
        const ProjectSyntaxUnit& unit,
        const std::string& sourcePath);
    void predeclareNode(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        const std::vector<std::string>& namespaceSegments,
        const std::string& sourcePath);
    void finalizePredeclarations(diagnostics::DiagnosticEngine& diagnostics);
    void bindUnit(
        const ProjectSyntaxUnit& unit,
        diagnostics::DiagnosticEngine& diagnostics);
    void bindScope(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* scopeNode,
        const std::vector<std::string>& namespaceSegments,
        const LookupEnvironment& inheritedEnvironment,
        diagnostics::DiagnosticEngine& diagnostics);
    void bindNode(
        const ProjectSyntaxUnit& unit,
        const ast::AstNode* node,
        const std::vector<std::string>& namespaceSegments,
        const LookupEnvironment& environment,
        diagnostics::DiagnosticEngine& diagnostics);
    void bindTypeSyntax(
        const ProjectSyntaxUnit& unit,
        const ast::TypeSyntax* syntax,
        const std::vector<std::string>& namespaceSegments,
        const LookupEnvironment& environment,
        diagnostics::DiagnosticEngine& diagnostics);

    std::vector<std::size_t> resolveTypeCandidates(
        std::size_t projectOrdinal,
        const std::vector<std::string>& namespaceSegments,
        const LookupEnvironment& environment,
        const std::string& writtenName) const;
    std::vector<std::size_t> resolveQualifiedTypeCandidates(
        std::size_t projectOrdinal,
        const std::vector<std::string>& namespaceSegments,
        const std::string& qualifiedName) const;
    std::vector<std::size_t> findExactTypeCandidates(
        std::size_t projectOrdinal,
        const std::string& fullyQualifiedName,
        bool currentModuleFirst) const;
    bool namespaceExistsVisible(std::size_t projectOrdinal, const std::string& namespaceName) const;
    std::optional<std::string> resolveNamespaceTarget(
        std::size_t projectOrdinal,
        const std::vector<std::string>& namespaceSegments,
        const std::string& target) const;
    std::vector<std::unique_ptr<ModuleDeclarationSpace>> modules_;
    std::vector<PendingTypeDeclaration> pendingTypes_;
    std::vector<NamedTypeDeclaration> namedTypes_;
    std::vector<TypeNameBinding> bindings_;
    std::vector<std::unordered_map<std::string, std::size_t>> typesByModule_;
    std::unordered_map<const ast::TypeSyntax*, std::size_t> bindingBySyntax_;
    bool built_ = false;
};

const char* namedTypeDeclarationKindName(NamedTypeDeclarationKind kind) noexcept;
const char* namedTypeAccessibilityName(NamedTypeAccessibility accessibility) noexcept;
const char* typeNameBindingKindName(TypeNameBindingKind kind) noexcept;
}
