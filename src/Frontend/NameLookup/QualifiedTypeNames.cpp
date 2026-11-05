#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/CppAliasBuiltinType.h"
#include "nativesharp/Frontend/CppAliasArrayType.h"
#include "nativesharp/Frontend/CppAliasRecordType.h"
#include "nativesharp/Frontend/QualifiedNamespaceTarget.h"
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
    if (!expandCppTypeAlias(unit, syntax, namespaceSegments, environment, diagnostics)) return;
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

bool ProjectNameLookup::expandCppTypeAlias(
    const ProjectSyntaxUnit& unit, const ast::TypeSyntax* syntax,
    const std::vector<std::string>& namespaceSegments, const LookupEnvironment& environment,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (cppImportedTypeAliases_.empty() || syntax->isFunction() ||
        lexer::isBuiltinTypeKeyword(syntax->baseTokenKind()) || !syntax->typeArguments().empty()) return true;
    const auto owner = syntax->originProjectOrdinal().value_or(unit.projectOrdinal());
    if (owner >= modules_.size()) return true;
    const auto effectiveNamespace = effectiveTypeNamespace(syntax, namespaceSegments);
    const auto enclosing = enclosingTypeSpellings(environment);
    QualifiedNameSite site;
    site.namespaceSegments = effectiveNamespace;
    site.enclosingTypeNames = enclosing;
    site.importedNamespaces = environment.importedNamespaces;
    site.aliases = &environment.aliases;
    const auto visible = modules_[owner]->visibleProjectOrdinals();
    const auto report = [&](diagnostics::DiagnosticId id, const std::string& message) {
        diagnostics.report(id, diagnostics::DiagnosticSeverity::Error, syntax->range(), message);
        return false;
    };
    for (const auto& rung : qualifiedNameLadder(site, syntax->baseName()))
    {
        const CppImportedTypeAlias* selected = nullptr;
        std::string selectedName;
        std::vector<std::size_t> nativeCandidates;
        for (const auto& spelling : rung.spellings)
        {
            const auto native = findExactTypeCandidates(owner, spelling, false);
            for (const auto ordinal : native)
                if (std::find(nativeCandidates.begin(), nativeCandidates.end(), ordinal) == nativeCandidates.end())
                    nativeCandidates.push_back(ordinal);
            for (const auto& alias : cppImportedTypeAliases_)
            {
                if (alias.declaration.insideAnonymousNamespace ||
                    (alias.projectOrdinal != owner &&
                     std::find(visible.begin(), visible.end(), alias.projectOrdinal) == visible.end())) continue;
                auto qualified = alias.declaration.qualifiedName;
                std::size_t pos = 0;
                while ((pos = qualified.find("::", pos)) != std::string::npos) qualified.replace(pos, 2, ".");
                if (qualified != spelling) continue;
                if (selected && (selectedName != qualified || selected->projectOrdinal != alias.projectOrdinal ||
                    selected->targetTriple != alias.targetTriple ||
                    selected->declaration.underlyingType.canonicalSpelling != alias.declaration.underlyingType.canonicalSpelling))
                    return report(diagnostics::DiagnosticId::AmbiguousTypeName,
                                  "conflicting C/C++ type alias '" + syntax->baseName() + "'");
                selected = &alias;
                selectedName = qualified;
            }
        }
        if (!selected)
        {
            if (!nativeCandidates.empty()) return true;
            continue;
        }
        const auto record = cppAliasRecordType(selected->declaration.underlyingType);
        if (!nativeCandidates.empty())
        {
            if (record && record->pointerDepth == 0 && !record->pointeeConst && nativeCandidates.size() == 1)
            {
                // Imported ordinal indices are finalized after binding. Inspect the already
                // aggregated declaration and its originating input instead of that later index.
                const auto* declaration = typeForOrdinal(nativeCandidates.front());
                if (declaration && declaration->projectOrdinal == selected->projectOrdinal &&
                    declaration->identity.fullyQualifiedName() == record->canonicalName &&
                    declaration->parts.size() == 1 && !declaration->parts.front().syntax)
                    for (const auto& imported : cppImportedTypes_)
                        if (imported.projectOrdinal == selected->projectOrdinal &&
                            imported.identity.key() == declaration->identity.key() &&
                            imported.sourcePath == declaration->parts.front().sourcePath) return true;
            }
            return report(diagnostics::DiagnosticId::AmbiguousTypeName,
                          "C/C++ type alias conflicts with a native type '" + syntax->baseName() + "'");
        }
        if (record && selected->targetTriple == "x86_64-pc-windows-msvc")
        {
            const CppImportedTypeDeclaration* target = nullptr;
            for (const auto& imported : cppImportedTypes_)
                if (imported.projectOrdinal == selected->projectOrdinal &&
                    imported.identity.fullyQualifiedName() == record->canonicalName)
                {
                    if (target) return report(diagnostics::DiagnosticId::AmbiguousTypeName,
                                              "ambiguous imported record for alias '" + syntax->baseName() + "'");
                    target = &imported;
                }
            if (!target || (record->pointeeConst && !syntax->modifiers().empty()) ||
                (syntax->isPointeeConst() && record->pointerDepth != 0))
                return report(diagnostics::DiagnosticId::InvalidUsingTarget,
                              "C/C++ record alias cannot yet be resolved safely: '" + syntax->baseName() + "'");
            auto* mutableSyntax = const_cast<ast::TypeSyntax*>(syntax);
            mutableSyntax->setBaseTokenKind(lexer::TokenKind::Identifier);
            mutableSyntax->setBaseName(record->name);
            mutableSyntax->setOriginProjectOrdinal(selected->projectOrdinal);
            mutableSyntax->setOriginNamespace("");
            mutableSyntax->prependModifiers(std::vector<ast::TypeModifierSyntax>(record->pointerDepth));
            mutableSyntax->setPointeeConst(mutableSyntax->isPointeeConst() || record->pointeeConst);
            return true;
        }
        const auto array = cppAliasArrayType(selected->declaration.underlyingType);
        if (array && selected->targetTriple == "x86_64-pc-windows-msvc")
        {
            if ((array->element.pointeeConst && !syntax->modifiers().empty()) ||
                (syntax->isPointeeConst() && array->element.pointerDepth != 0))
                return report(diagnostics::DiagnosticId::InvalidUsingTarget,
                              "C/C++ array alias has qualifiers that cannot yet be represented: '" +
                              syntax->baseName() + "'");
            auto* mutableSyntax = const_cast<ast::TypeSyntax*>(syntax);
            auto& arena = const_cast<ProjectSyntaxUnit&>(unit).arena();
            std::vector<ast::TypeModifierSyntax> modifiers(array->element.pointerDepth);
            for (auto dimension = array->dimensions.rbegin(); dimension != array->dimensions.rend(); ++dimension)
            {
                auto* count = arena.make<ast::AstNode>(ast::AstNodeKind::IntegerLiteralExpression, syntax->range());
                count->setTokenKind(lexer::TokenKind::IntegerLiteral);
                count->setText(std::to_string(*dimension));
                count->setIntegerValue(*dimension);
                ast::TypeModifierSyntax modifier;
                modifier.kind = ast::TypeModifierSyntaxKind::Array;
                modifier.arrayCount = count;
                modifier.range = syntax->range();
                modifiers.push_back(modifier);
            }
            mutableSyntax->setBaseTokenKind(array->element.token);
            mutableSyntax->setBaseName(std::string(array->element.name));
            mutableSyntax->prependModifiers(modifiers);
            mutableSyntax->setPointeeConst(mutableSyntax->isPointeeConst() || array->element.pointeeConst);
            return true;
        }
        const auto mapped = cppAliasBuiltinType(selected->declaration.underlyingType);
        if (selected->targetTriple != "x86_64-pc-windows-msvc" || !mapped ||
            (mapped->pointeeConst && !syntax->modifiers().empty()) ||
            (syntax->isPointeeConst() && mapped->pointerDepth != 0))
            return report(diagnostics::DiagnosticId::InvalidUsingTarget,
                          "C/C++ type alias cannot yet be represented without losing type information: '" +
                          syntax->baseName() + "'");
        auto* mutableSyntax = const_cast<ast::TypeSyntax*>(syntax);
        mutableSyntax->setBaseTokenKind(mapped->token);
        mutableSyntax->setBaseName(std::string(mapped->name));
        mutableSyntax->prependModifiers(std::vector<ast::TypeModifierSyntax>(mapped->pointerDepth));
        mutableSyntax->setPointeeConst(mutableSyntax->isPointeeConst() || mapped->pointeeConst);
        return true;
    }
    return true;
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
    return resolveQualifiedNamespaceTarget(namespaceSegments, target, [&](const std::string& candidate) {
        return namespaceExistsVisible(projectOrdinal, candidate);
    });
}

}
