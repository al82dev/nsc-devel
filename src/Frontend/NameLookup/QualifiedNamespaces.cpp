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
std::vector<std::string> splitQualifiedName(const std::string& name)
{
    std::vector<std::string> segments;
    std::size_t begin = 0;
    while (begin <= name.size())
    {
        const std::size_t dot = name.find('.', begin);
        const std::size_t end = dot == std::string::npos ? name.size() : dot;
        if (end > begin)
        {
            segments.push_back(name.substr(begin, end - begin));
        }
        if (dot == std::string::npos)
        {
            break;
        }
        begin = dot + 1;
    }
    return segments;
}
}

void ProjectNameLookup::finalizeNamespaceDeclarations()
{
    std::sort(namespaceFunctions_.begin(), namespaceFunctions_.end(), namespaceFunctionLess);
    std::sort(namespaceStates_.begin(), namespaceStates_.end(), namespaceStateLess);
    for (std::size_t ordinal = 0; ordinal < namespaceFunctions_.size(); ++ordinal)
    {
        const NamespaceFunctionDeclaration& function = namespaceFunctions_[ordinal];
        std::vector<std::string> segments = splitQualifiedName(function.namespaceName);
        modules_[function.projectOrdinal]->globalNamespace()
            .ensureNamespace(segments).addFunctionOrdinal(ordinal);
    }
}

void ProjectNameLookup::predeclareNamespace(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::vector<std::string>& namespaceSegments,
    const std::vector<compilation::TypeNameSegment>& containingTypes,
    const std::string& sourcePath,
    UnitDeclarationIndex& index) const
{
    std::vector<std::string> nestedNamespace = namespaceSegments;
    const std::vector<std::string> declaredSegments = splitQualifiedName(node->text());
    nestedNamespace.insert(
        nestedNamespace.end(), declaredSegments.begin(), declaredSegments.end());
    index.namespaces.push_back(nestedNamespace);
    for (const ast::AstNode* child : node->children())
        predeclareNode(unit, child, nestedNamespace, containingTypes, sourcePath, index);
}

void ProjectNameLookup::bindScope(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* scopeNode,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& inheritedEnvironment,
    diagnostics::DiagnosticEngine& diagnostics,
    UnitBindingIndex& index)
{
    LookupEnvironment environment = inheritedEnvironment;
    bindUsingDirectives(unit, scopeNode, namespaceSegments, environment, diagnostics);

    for (const ast::AstNode* child : scopeNode->children())
    {
        if (child->kind() == ast::AstNodeKind::UsingNamespaceDirective ||
            child->kind() == ast::AstNodeKind::UsingAliasDirective)
        {
            continue;
        }
        if (child->kind() == ast::AstNodeKind::NamespaceDeclaration)
        {
            std::vector<std::string> nestedNamespace = namespaceSegments;
            const std::vector<std::string> segments = splitQualifiedName(child->text());
            nestedNamespace.insert(nestedNamespace.end(), segments.begin(), segments.end());
            bindScope(unit, child, nestedNamespace, environment, diagnostics, index);
            continue;
        }
        bindNode(unit, child, namespaceSegments, environment, diagnostics, index);
    }
}

bool ProjectNameLookup::bindNamespaceDeclaration(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& environment,
    diagnostics::DiagnosticEngine& diagnostics,
    UnitBindingIndex& index)
{
    if (node->kind() != ast::AstNodeKind::NamespaceDeclaration) return false;
    std::vector<std::string> nestedNamespace = namespaceSegments;
    const std::vector<std::string> segments = splitQualifiedName(node->text());
    nestedNamespace.insert(nestedNamespace.end(), segments.begin(), segments.end());
    bindScope(unit, node, nestedNamespace, environment, diagnostics, index);
    return true;
}

std::vector<std::string> ProjectNameLookup::effectiveTypeNamespace(
    const ast::TypeSyntax* syntax,
    const std::vector<std::string>& namespaceSegments)
{
    // Companion override for the lexical namespace -- see TypeSyntax::originNamespace. Without
    // this, "Payload" would still be searched under the TEMPLATE's own namespace (e.g.
    // "NativeSharp.Core.Collections.Generic"), never finding a type declared only in the
    // requesting project's OWN namespace (e.g. "Stage52LibTest").
    if (syntax->originNamespace().has_value())
        return splitQualifiedName(syntax->originNamespace().value());
    // Older/current clones from a consumer in the global namespace carry the project
    // override but no namespace string. That still means "global namespace of the origin
    // project", never "template namespace".
    if (syntax->originProjectOrdinal().has_value()) return {};
    return namespaceSegments;
}

std::vector<std::size_t> ProjectNameLookup::resolveTypeCandidates(
    std::size_t projectOrdinal,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& environment,
    const std::string& writtenName) const
{
    const std::size_t moduleSeparator = writtenName.find("::");
    if (moduleSeparator != std::string::npos)
        return resolveModuleQualifiedTypeCandidates(
            projectOrdinal, writtenName, moduleSeparator);

    const std::vector<std::string> nameSegments = splitQualifiedName(writtenName);
    if (nameSegments.empty() || projectOrdinal >= typesByModule_.size()) return {};

    // LA ESCALA VIENE DE FUERA, de `qualifiedNameLadder`. Este bucle NO decide el orden: lo
    // recorre. Lo que decide, y lo que sigue viviendo aqui, es COMO se sondea cada escalon --que
    // tabla, y con que regla de visibilidad--, porque eso es propio del modelo de nombres y no
    // tiene sentido en la fase generica, que sondea plantillas.
    //
    // Antes el orden estaba escrito aqui y COPIADO, mas simple y por tanto distinto, dentro de
    // `ProjectGenericModel`. Ver QualifiedNameLadder.h.
    const bool simpleName = nameSegments.size() == 1 &&
        environment.aliases.find(nameSegments.front()) == environment.aliases.end();
    QualifiedNameSite site;
    site.namespaceSegments = namespaceSegments;
    site.enclosingTypeNames = enclosingTypeSpellings(environment);
    site.aliases = &environment.aliases;
    site.importedNamespaces = environment.importedNamespaces;
    for (const QualifiedNameRung& rung : qualifiedNameLadder(site, writtenName))
    {
        std::vector<std::size_t> found;
        for (const std::string& candidate : rung.spellings)
            appendQualifiedTypeCandidate(
                found, projectOrdinal, environment, candidate, rung.tier, simpleName);
        if (!found.empty()) return found;
    }
    return {};
}

bool ProjectNameLookup::namespaceExistsVisible(
    std::size_t projectOrdinal,
    const std::string& namespaceName) const
{
    const std::vector<std::string> segments = splitQualifiedName(namespaceName);
    if (projectOrdinal >= modules_.size())
    {
        return false;
    }

    if (modules_[projectOrdinal]->globalNamespace().findNamespace(segments) != nullptr)
    {
        return true;
    }
    for (const std::size_t visibleOrdinal : modules_[projectOrdinal]->visibleProjectOrdinals())
    {
        if (modules_[visibleOrdinal]->globalNamespace().findNamespace(segments) != nullptr)
        {
            return true;
        }
    }
    return false;
}

}
