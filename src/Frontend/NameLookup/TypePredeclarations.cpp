#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "../Internal/LookupEnvironment.h"
#include "../Internal/ProjectNameLookupFrame.h"

#include "nativesharp/Frontend/QualifiedNameLadder.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Compilation/PartialTypeAggregator.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <deque>
#include <filesystem>
#include <functional>
#include <iterator>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
bool isNamedTypeDeclaration(ast::AstNodeKind kind) noexcept
{
    return kind == ast::AstNodeKind::ClassDeclaration ||
        kind == ast::AstNodeKind::InterfaceDeclaration ||
        kind == ast::AstNodeKind::StructDeclaration ||
        kind == ast::AstNodeKind::EnumDeclaration ||
        kind == ast::AstNodeKind::DelegateDeclaration;
}

NamedTypeDeclarationKind namedTypeKind(ast::AstNodeKind kind) noexcept
{
    if (kind == ast::AstNodeKind::ClassDeclaration)
    {
        return NamedTypeDeclarationKind::Class;
    }
    if (kind == ast::AstNodeKind::InterfaceDeclaration)
    {
        return NamedTypeDeclarationKind::Interface;
    }
    if (kind == ast::AstNodeKind::EnumDeclaration)
    {
        return NamedTypeDeclarationKind::Enum;
    }
    if (kind == ast::AstNodeKind::DelegateDeclaration)
    {
        return NamedTypeDeclarationKind::Delegate;
    }
    return NamedTypeDeclarationKind::Struct;
}
}

void ProjectNameLookup::predeclareExtension(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::vector<std::string>& namespaceSegments,
    const std::vector<compilation::TypeNameSegment>& containingTypes,
    const std::string& sourcePath,
    UnitDeclarationIndex& index) const
{
    for (const ast::AstNode* child : node->children())
        predeclareNode(unit, child, namespaceSegments, containingTypes, sourcePath, index);
}

NamedTypeAccessibility ProjectNameLookup::predeclaredTypeAccessibility(
    const ast::AstNode* node,
    bool isNested)
{
    // NSC-LIMIT-009. EL VALOR POR OMISION DEPENDE DEL SITIO, y esa es toda la regla: un tipo
    // de namespace sin modificador es `internal` --visible en su modulo-- y uno ANIDADO sin
    // modificador es `private`, visible solo dentro de su contenedor. Es la regla de C#, y
    // convertir el segundo en el primero seria publicar dentro del modulo cada tipo interno
    // que alguien escribiera para uso propio.
    NamedTypeAccessibility accessibility =
        isNested ? NamedTypeAccessibility::Private : NamedTypeAccessibility::Internal;
    if (node->tokenKind() == lexer::TokenKind::KwPublic)
        accessibility = NamedTypeAccessibility::Public;
    else if (node->tokenKind() == lexer::TokenKind::KwInternal)
        accessibility = node->auxiliaryText().find("visibility=package") != std::string::npos
            ? NamedTypeAccessibility::Package
            : NamedTypeAccessibility::Internal;
    else if (node->tokenKind() == lexer::TokenKind::KwProtected)
        accessibility = NamedTypeAccessibility::Protected;
    else if (node->tokenKind() == lexer::TokenKind::KwPrivate)
        accessibility = NamedTypeAccessibility::Private;
    return accessibility;
}

NamedTypeDeclarationPart ProjectNameLookup::predeclaredTypePart(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::string& sourcePath,
    bool isNested,
    NamedTypeAccessibility accessibility)
{
    const bool hasExplicitAccessibility =
        node->tokenKind() == lexer::TokenKind::KwPublic ||
        node->tokenKind() == lexer::TokenKind::KwInternal ||
        node->tokenKind() == lexer::TokenKind::KwPrivate ||
        node->tokenKind() == lexer::TokenKind::KwProtected;
    return NamedTypeDeclarationPart{
        namedTypeKind(node->kind()), accessibility, hasExplicitAccessibility, isNested,
        (node->kind() == ast::AstNodeKind::ClassDeclaration ||
         node->kind() == ast::AstNodeKind::InterfaceDeclaration) && node->flag(),
        node->kind() == ast::AstNodeKind::ClassDeclaration &&
            node->hasModifier(lexer::TokenKind::KwAbstract),
        node->kind() == ast::AstNodeKind::ClassDeclaration &&
            node->hasModifier(lexer::TokenKind::KwSealed),
        node->kind() == ast::AstNodeKind::ClassDeclaration &&
            node->hasModifier(lexer::TokenKind::KwStatic),
        unit.isPrecompiled(), unit.projectOrdinal(), unit.sourceOrdinal(), unit.fileId(),
        node->range(), node, sourcePath};
}

void ProjectNameLookup::predeclareNestedTypes(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::vector<std::string>& namespaceSegments,
    const std::vector<compilation::TypeNameSegment>& containingTypes,
    const std::string& sourcePath,
    UnitDeclarationIndex& index) const
{
    // NSC-LIMIT-009. Y AHORA SE BAJA. La version anterior devolvia aqui, que es exactamente
    // por lo que un tipo dentro de otro no existia para nadie: se parseaba, se colgaba del
    // arbol, y ninguna fase volvia a mirarlo.
    //
    // La aridad de cada segmento se toma del nodo --no se supone cero-- aunque hoy solo pueda
    // valer cero: el parser rechaza un anidado generico, y esta linea es lo que hara que el
    // dia que deje de rechazarlo la identidad salga completa en vez de ambigua.
    std::vector<compilation::TypeNameSegment> nestedChain = containingTypes;
    nestedChain.push_back(compilation::TypeNameSegment{
        node->text(), static_cast<std::uint32_t>(node->genericParameters().size())});
    for (const ast::AstNode* child : node->children())
    {
        if (isNamedTypeDeclaration(child->kind()))
            predeclareNode(unit, child, namespaceSegments, nestedChain, sourcePath, index);
    }
}

void ProjectNameLookup::predeclareNamedType(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::vector<std::string>& namespaceSegments,
    const std::vector<compilation::TypeNameSegment>& containingTypes,
    const std::string& sourcePath,
    UnitDeclarationIndex& index) const
{
    const bool isNested = !containingTypes.empty();
    compilation::TypeIdentity identity(
        std::string(unit.moduleName()), namespaceSegments, containingTypes, node->text(), 0);
    NamedTypeDeclarationPart part = predeclaredTypePart(
        unit, node, sourcePath, isNested,
        predeclaredTypeAccessibility(node, isNested));
    index.pendingTypes.push_back(PendingTypeDeclaration{std::move(identity), std::move(part)});
    predeclareNestedTypes(
        unit, node, namespaceSegments, containingTypes, sourcePath, index);
}

void ProjectNameLookup::predeclareCompilationUnit(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::vector<std::string>& namespaceSegments,
    const std::vector<compilation::TypeNameSegment>& containingTypes,
    const std::string& sourcePath,
    UnitDeclarationIndex& index) const
{
    for (const ast::AstNode* child : node->children())
        predeclareNode(unit, child, namespaceSegments, containingTypes, sourcePath, index);
}

void ProjectNameLookup::predeclareNode(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::vector<std::string>& namespaceSegments,
    const std::vector<compilation::TypeNameSegment>& containingTypes,
    const std::string& sourcePath,
    UnitDeclarationIndex& index) const
{
    if (node == nullptr) return;
    if (!node->genericParameters().empty() && isNamedTypeDeclaration(node->kind())) return;
    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
    {
        predeclareNamespace(
            unit, node, namespaceSegments, containingTypes, sourcePath, index);
        return;
    }
    if ((node->kind() == ast::AstNodeKind::FunctionDeclaration ||
         (node->kind() == ast::AstNodeKind::MethodDeclaration &&
          node->auxiliaryText().find("ns3-extension-static") != std::string::npos)) &&
        node->genericParameters().empty())
    {
        predeclareNamespaceFunction(unit, node, namespaceSegments, sourcePath, index);
        return;
    }
    if (node->kind() == ast::AstNodeKind::GlobalVariableDeclaration &&
        !namespaceSegments.empty())
    {
        predeclareNamespaceState(unit, node, namespaceSegments, sourcePath, index);
        return;
    }
    if (node->kind() == ast::AstNodeKind::ExtensionDeclaration)
    {
        predeclareExtension(
            unit, node, namespaceSegments, containingTypes, sourcePath, index);
        return;
    }
    if (isNamedTypeDeclaration(node->kind()))
    {
        predeclareNamedType(
            unit, node, namespaceSegments, containingTypes, sourcePath, index);
        return;
    }
    if (node->kind() == ast::AstNodeKind::CompilationUnit)
        predeclareCompilationUnit(
            unit, node, namespaceSegments, containingTypes, sourcePath, index);
}

std::optional<std::size_t> ProjectNameLookup::enclosingTypeOrdinal(
    const ast::AstNode* node) const
{
    if (!isNamedTypeDeclaration(node->kind())) return std::nullopt;
    const auto ordinal = typeOrdinalBySyntax_.find(node);
    return ordinal == typeOrdinalBySyntax_.end()
        ? std::nullopt : std::optional<std::size_t>(ordinal->second);
}

}
