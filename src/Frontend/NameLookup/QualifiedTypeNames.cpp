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
std::string joinSegments(std::span<const std::string> segments)
{
    std::string result;
    for (const std::string& segment : segments)
    {
        if (!result.empty())
        {
            result.push_back('.');
        }
        result += segment;
    }
    return result;
}

std::string appendQualified(std::span<const std::string> prefix, const std::string& suffix)
{
    std::string result = joinSegments(prefix);
    if (!result.empty() && !suffix.empty())
    {
        result.push_back('.');
    }
    result += suffix;
    return result;
}
}

void ProjectNameLookup::predeclareNamespaceFunction(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::vector<std::string>& namespaceSegments,
    const std::string& sourcePath,
    UnitDeclarationIndex& index)
{
    index.namespaceFunctions.push_back(NamespaceFunctionDeclaration{
        node->tokenKind() == lexer::TokenKind::KwPublic
            ? NamespaceFunctionAccessibility::Public
            : NamespaceFunctionAccessibility::Internal,
        unit.projectOrdinal(),
        unit.sourceOrdinal(),
        unit.fileId(),
        node->range(),
        node,
        sourcePath,
        std::string(unit.moduleName()),
        joinSegments(namespaceSegments),
        node->text(),
        node->first() != nullptr,
        unit.isPrecompiled()});
}

void ProjectNameLookup::predeclareNamespaceState(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::vector<std::string>& namespaceSegments,
    const std::string& sourcePath,
    UnitDeclarationIndex& index)
{
    index.namespaceStates.push_back(NamespaceStateDeclaration{
        node->tokenKind() == lexer::TokenKind::KwPublic
            ? NamespaceFunctionAccessibility::Public
            : NamespaceFunctionAccessibility::Internal,
        unit.projectOrdinal(),
        unit.sourceOrdinal(),
        unit.fileId(),
        node->range(),
        node,
        sourcePath,
        std::string(unit.moduleName()),
        joinSegments(namespaceSegments),
        node->text(),
        node->flag(),
        unit.isPrecompiled()});
}

void ProjectNameLookup::bindTypeSyntax(
    const ProjectSyntaxUnit& unit,
    const ast::TypeSyntax* syntax,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& environment,
    diagnostics::DiagnosticEngine& diagnostics,
    UnitBindingIndex& index)
{
    if (syntax == nullptr || index.bindingBySyntax.contains(syntax)) return;
    std::string constType;
    std::string constValue;
    if (language::parseConstGenericArgumentMarker(syntax->baseName(), constType, constValue))
        return;
    // `var`'s marker TypeSyntax (see Parser::parseVariableDeclaration) has no real named type to
    // resolve -- its real type is inferred entirely from the initializer expression, inside
    // ProjectMethodBodyModel::analyzeVariable, which never looks this binding up. Binding it here
    // like an ordinary identifier type would incorrectly report "unknown type name 'var'" (there
    // is, correctly, no class/interface/struct actually named "var").
    if (syntax->baseTokenKind() == lexer::TokenKind::KwVar) return;
    // The name the programmer actually WROTE, captured before any expansion: diagnostics and the
    // binding record must keep saying `PointAlias`, not the type it turned out to mean.
    const std::string writtenName = syntax->baseName();
    expandTypeAlias(syntax, environment, diagnostics);
    bindChildTypeSyntaxes(unit, syntax, namespaceSegments, environment, diagnostics, index);
    // Stage 52 fix: a generic type argument substituted into a specialization cloned from a
    // referenced project's template (e.g. List<MyOwnClass> where List<T> lives in a shared
    // library) carries an explicit origin-project override -- see TypeSyntax::originProjectOrdinal
    // for the full explanation. Fall back to the enclosing unit's own project ordinal for every
    // ordinary (non-substituted) type syntax node, exactly as before.
    const std::size_t effectiveProjectOrdinal =
        syntax->originProjectOrdinal().value_or(unit.projectOrdinal());
    const std::vector<std::string> effectiveNamespaceSegments =
        effectiveTypeNamespace(syntax, namespaceSegments);
    TypeNameBinding binding;
    binding.syntax = syntax;
    binding.projectOrdinal = effectiveProjectOrdinal;
    binding.fileId = unit.fileId();
    binding.namespaceName = joinSegments(effectiveNamespaceSegments);
    binding.writtenName = writtenName;
    binding.enclosingTypeOrdinals = environment.enclosingTypeOrdinals;
    const bool portableSimd = syntax->baseName() == "simd" || syntax->baseName() == "mask";
    if (isBuiltinTypeSurface(syntax, portableSimd))
        validateBuiltinTypeBinding(syntax, portableSimd, binding, diagnostics);
    else
        resolveTypeNameBinding(
            unit, syntax, effectiveNamespaceSegments, environment, binding, diagnostics);
    const std::size_t ordinal = index.bindings.size();
    index.bindings.push_back(std::move(binding));
    index.bindingBySyntax.emplace(syntax, ordinal);
}

std::optional<std::size_t> ProjectNameLookup::findInaccessibleNestedType(
    std::size_t projectOrdinal,
    const std::vector<std::string>& namespaceSegments,
    const std::string& writtenName) const
{
    if (projectOrdinal >= nestedSourceNamesByModule_.size())
    {
        return std::nullopt;
    }
    for (std::size_t depth = namespaceSegments.size() + 1; depth > 0; --depth)
    {
        const std::string candidate = appendQualified(
            std::span<const std::string>(namespaceSegments.data(), depth - 1),
            writtenName);
        const auto iterator = nestedSourceNamesByModule_[projectOrdinal].find(candidate);
        if (iterator != nestedSourceNamesByModule_[projectOrdinal].end())
        {
            return iterator->second;
        }
    }
    return std::nullopt;
}

std::optional<std::string> ProjectNameLookup::resolveNamespaceTarget(
    std::size_t projectOrdinal,
    const std::vector<std::string>& namespaceSegments,
    const std::string& target) const
{
    for (std::size_t depth = namespaceSegments.size() + 1; depth > 0; --depth)
    {
        const std::size_t prefixCount = depth - 1;
        const std::string candidate = appendQualified(
            std::span<const std::string>(namespaceSegments.data(), prefixCount),
            target);
        if (namespaceExistsVisible(projectOrdinal, candidate))
        {
            return candidate;
        }
    }
    return std::nullopt;
}

}
