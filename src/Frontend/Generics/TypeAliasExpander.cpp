#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "../Internal/CloneDepthGuard.h"
#include "../Internal/ByteReader.h"
#include "../Internal/EnumConstResolver.h"
#include "../Internal/SpecializationWaveTimer.h"
#include "../Internal/InterfaceFingerprint.h"
#include "../Internal/ImplementationFingerprint.h"
#include "../Internal/LocatedNode.h"
#include "../Internal/MirFunctionKey.h"
#include "../Internal/MirCacheRecord.h"
#include "../Internal/ProjectGenericDependencyFrame.h"
#include "../Internal/ProjectGenericBodyFrame.h"
#include "../Internal/ProjectGenericAliasFrame.h"
#include "../Internal/ProjectGenericChildWork.h"
#include "../Internal/ProjectGenericCollectFrame.h"
#include "../Internal/ProjectGenericProcessFrame.h"
#include "../Internal/ProjectGenericReservedDemand.h"
#include "../Internal/TypeWork.h"
#include "nativesharp/Frontend/ProjectOpaqueResultModel.h"

#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "GenericModelSupport.h"
#include "Support/TemplateClassification.h"
#include "Support/TemplateSpelling.h"
#include "Support/ConstGenericArguments.h"
#include "Support/GenericPatterns.h"
#include "Cache/TemplateCatalogCodec.h"
namespace nativesharp::frontend
{
using internal::appendOwner;
using internal::computeTemplateSourceFingerprint;
using internal::constArgumentExpression;
using internal::constGenericParameterTypeKey;
using internal::decodeConstGenericArgument;
using internal::deserializeAstNodeTree;
using internal::enclosingTypeSpellings;
using internal::genericArityAccepts;
using internal::genericAritySpelling;
using internal::genericTemplateAccessibility;
using internal::genericTypeTemplateKind;
using internal::isGenericMethodTemplateNode;
using internal::isGenericTypeTemplateNode;
using internal::isNamedTypeDeclaration;
using internal::isStructuralGenericSurface;
using internal::methodSpecializationFamilyKey;
using internal::normalizeConstExpressionArgument;
using internal::qualifiedName;
using internal::receiverSourceSpelling;
using internal::restoreCatalogEntry;
using internal::serializeAstNodeTree;
using internal::serializeCatalogEntry;
using internal::serializeMirCacheRecord;
using internal::splitDotted;
using internal::stableHashSuffix;
using internal::templateCacheKey;
using internal::templateSourceSpelling;
using internal::trailingPackIndex;
using internal::valueParameterCount;
using internal::ByteReader;
using internal::EnumConstResolver;
using internal::MirCacheRecord;
using internal::SpecializationWaveTimer;
#if 0 // INSTRUMENTATION CODE
using internal::genericNodeDetail;
using internal::genericTypeDetail;
using internal::genericUnitDetail;
using internal::GenericWorkProgress;
#endif // INSTRUMENTATION CODE
ast::TypeSyntax* ProjectGenericModel::cloneAliasType(
    ast::AstArena& arena,
    const ast::TypeSyntax* source,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    bool& ownershipConflict)
{
    if (source == nullptr) return nullptr;
#if 0 // INSTRUMENTATION CODE
    const GenericWorkProgress progress(
        telemetry_, "generics.clone-alias-type", genericTypeDetail(*source));
#endif // INSTRUMENTATION CODE

    ast::TypeSyntax* result = nullptr;
    const auto substitution = substitutions.find(source->baseName());
    if (substitution != substitutions.end() && source->typeArguments().empty())
    {
        result = cloneAliasType(arena, substitution->second, {}, ownershipConflict);
    }
    else
    {
        result = arena.make<ast::TypeSyntax>(
            source->range(), source->baseTokenKind(), source->baseName());
        result->setTuple(source->isTuple());
        result->setFunction(source->isFunction());
        result->setFunctionCarrier(source->functionCarrier());
        result->setFunctionCallingConvention(source->functionCallingConvention());
        result->setFunctionVariadic(source->functionIsVariadic());
        result->setFunctionUnsafe(source->functionIsUnsafe());
        result->setFunctionNoHeap(source->functionIsNoHeap());
        result->setFunctionAsync(source->functionIsAsync());
        result->setFunctionNullable(source->functionIsNullable());
        result->setFunctionLifetimeName(source->functionLifetimeName());
        result->setFunctionReturnType(cloneAliasType(
            arena, source->functionReturnType(), substitutions, ownershipConflict));
        result->setFunctionErrorType(cloneAliasType(
            arena, source->functionErrorType(), substitutions, ownershipConflict));
        for (std::size_t index = 0; index < source->typeArguments().size(); ++index)
        {
            ast::TypeSyntax* argument = cloneAliasType(
                arena, source->typeArguments()[index], substitutions, ownershipConflict);
            if (source->isTuple())
            {
                const std::string name = index < source->tupleElementNames().size()
                    ? source->tupleElementNames()[index] : std::string{};
                result->addTupleElement(argument, name);
            }
            else
            {
                result->addTypeArgument(argument);
            }
        }
        if (source->originProjectOrdinal().has_value())
            result->setOriginProjectOrdinal(source->originProjectOrdinal().value());
        if (source->originNamespace().has_value())
            result->setOriginNamespace(source->originNamespace().value());
    }

    if (source->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None)
    {
        if (result->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None &&
            result->ownershipQualifier() != source->ownershipQualifier())
            ownershipConflict = true;
        else
            result->setOwnershipQualifier(source->ownershipQualifier());
    }
    result->setLifetimeName(source->lifetimeName());
    for (const ast::TypeModifierSyntax& modifier : source->modifiers())
    {
        // Array extents are immutable expression syntax owned by the same source unit.  Alias
        // substitution changes types, never value parameters, so sharing this pointer is exact.
        result->addModifier(ast::TypeModifierSyntax{
            modifier.kind, modifier.arrayCount, modifier.range});
    }
    result->setPackExpansion(source->isPackExpansion());
    result->setDynamicTrait(source->isDynamicTrait());
    result->setOpaqueResult(source->isOpaqueResult());
    result->setOpaqueResultTraitName(source->opaqueResultTraitName());
    result->setPointeeConst(source->isPointeeConst());
    return result;
}

bool ProjectGenericModel::applyExpandedAliasSyntax(
    ast::TypeSyntax* syntax,
    ast::TypeSyntax* expanded,
    const ast::AstNode* declaration,
    ast::OwnershipQualifierSyntaxKind writtenOwnership,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const ast::OwnershipQualifierSyntaxKind targetOwnership = expanded->ownershipQualifier();
    if (writtenOwnership != ast::OwnershipQualifierSyntaxKind::None &&
        targetOwnership != ast::OwnershipQualifierSyntaxKind::None &&
        writtenOwnership != targetOwnership)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "type alias '" + declaration->text() +
                "' conflicts with the ownership qualifier written at its use site");
        return false;
    }

    syntax->setBaseTokenKind(expanded->baseTokenKind());
    syntax->setBaseName(expanded->baseName());
    syntax->setDynamicTrait(syntax->isDynamicTrait() || expanded->isDynamicTrait());
    syntax->setOpaqueResult(syntax->isOpaqueResult() || expanded->isOpaqueResult());
    if (syntax->opaqueResultTraitName().empty())
        syntax->setOpaqueResultTraitName(expanded->opaqueResultTraitName());
    if (writtenOwnership == ast::OwnershipQualifierSyntaxKind::None)
        syntax->setOwnershipQualifier(targetOwnership);
    const std::vector<ast::TypeSyntax*> expandedArguments(
        expanded->typeArguments().begin(), expanded->typeArguments().end());
    syntax->clearTypeArguments();
    for (ast::TypeSyntax* argument : expandedArguments) syntax->addTypeArgument(argument);
    // Target/substituted modifiers are inside modifiers written on the alias use.  For example,
    // `using P<T> = T*; P<int>[]` becomes `int*[]`.
    syntax->prependModifiers(expanded->modifiers());
    if (expanded->originProjectOrdinal().has_value())
        syntax->setOriginProjectOrdinal(expanded->originProjectOrdinal().value());
    if (expanded->originNamespace().has_value())
        syntax->setOriginNamespace(expanded->originNamespace().value());
    return true;
}

bool ProjectGenericModel::expandTypeAliases(
    ProjectSyntaxUnit& unit,
    ast::TypeSyntax* syntax,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics,
    std::vector<std::string>& expansionStack)
{
    if (syntax == nullptr) return true;
    std::string constantType;
    std::string constantValue;
    if (decodeConstGenericArgument(syntax, constantType, constantValue)) return true;
    bool ok = true;
    for (ast::TypeSyntax* argument : syntax->typeArguments())
        ok = expandTypeAliases(unit, argument, scope, diagnostics, expansionStack) && ok;

    if (lexer::isBuiltinTypeKeyword(syntax->baseTokenKind()) ||
        scope.typeParameters.read().contains(syntax->baseName()))
        return ok;

    const auto found = scope.typeAliases.read().find(syntax->baseName());
    if (found == scope.typeAliases.read().end() || found->second == nullptr) return ok;
    const ast::AstNode* declaration = found->second;

    if (std::find(expansionStack.begin(), expansionStack.end(), syntax->baseName()) !=
        expansionStack.end())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidUsingTarget,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "using alias '" + syntax->baseName() + "' expands cyclically");
        return false;
    }

    if (syntax->typeArguments().size() != declaration->genericParameters().size())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "type alias '" + declaration->text() + "' expects " +
                std::to_string(declaration->genericParameters().size()) +
                " type argument(s) but received " +
                std::to_string(syntax->typeArguments().size()));
        return false;
    }

    std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
    for (std::size_t index = 0; index < declaration->genericParameters().size(); ++index)
        substitutions.emplace(declaration->genericParameters()[index], syntax->typeArguments()[index]);

    expansionStack.push_back(syntax->baseName());
    bool targetOwnershipConflict = false;
    ast::TypeSyntax* expanded =
        cloneAliasType(
            unit.arena(), declaration->typeSyntax(), substitutions, targetOwnershipConflict);
    if (targetOwnershipConflict)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "type alias '" + declaration->text() +
                "' applies a conflicting ownership qualifier to a type argument");
        expansionStack.pop_back();
        return false;
    }
    ok = expandTypeAliases(unit, expanded, scope, diagnostics, expansionStack) && ok;
    expansionStack.pop_back();
    if (!ok || expanded == nullptr) return false;

    const ast::OwnershipQualifierSyntaxKind writtenOwnership = syntax->ownershipQualifier();
    return applyExpandedAliasSyntax(
        syntax, expanded, declaration, writtenOwnership, diagnostics);
}

bool ProjectGenericModel::expandAliasesForNode(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool nodeOk = true;
    // Normalize every alias target in its declaration scope first.  This makes a chained alias
    // immune to a same-named alias that may shadow it only at a later use site.
    for (const ast::AstNode* child : node->children())
    {
        if (child == nullptr || child->kind() != ast::AstNodeKind::UsingAliasDirective ||
            child->typeSyntax() == nullptr ||
            (child->genericParameters().empty() && !child->auxiliaryText().empty()))
            continue;
        GenericLookupScope aliasScope = scope;
        for (const ast::GenericParameterSyntax& parameter : child->genericParameterSyntaxes())
            if (parameter.kind == ast::GenericParameterSyntaxKind::Type ||
                parameter.kind == ast::GenericParameterSyntaxKind::TypePack)
                aliasScope.typeParameters.write().insert(parameter.name);
        std::vector<std::string> stack{child->text()};
        nodeOk = expandTypeAliases(
                 unit, child->typeSyntax(), aliasScope, diagnostics, stack) && nodeOk;
    }

    if (node->kind() == ast::AstNodeKind::UsingAliasDirective) return nodeOk;
    std::vector<std::string> stack;
    nodeOk = expandTypeAliases(unit, node->typeSyntax(), scope, diagnostics, stack) && nodeOk;
    for (ast::TypeSyntax* syntax : node->typeSyntaxes())
    {
        stack.clear();
        nodeOk = expandTypeAliases(unit, syntax, scope, diagnostics, stack) && nodeOk;
    }
    for (const ast::GenericConstraintClause& clause : node->genericConstraints())
    {
        for (ast::TypeSyntax* constraint : clause.typeConstraints)
        {
            stack.clear();
            nodeOk = expandTypeAliases(unit, constraint, scope, diagnostics, stack) && nodeOk;
        }
        if (clause.equalityConstraint != nullptr)
        {
            stack.clear();
            nodeOk = expandTypeAliases(
                unit, clause.equalityConstraint, scope, diagnostics, stack) && nodeOk;
        }
    }
    return nodeOk;
}

bool ProjectGenericModel::expandAliasesInNode(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    std::string namespaceName,
    GenericLookupScope scope,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // NSC305-G05. Iterative pre-order traversal with an explicit stack; the per-node logic is
    // unchanged, only the recursion into first/second/third/children is removed.
    std::vector<AliasFrame> frames;
    frames.push_back(AliasFrame{node, std::move(namespaceName), std::move(scope)});
    bool ok = true;
    while (!frames.empty())
    {
        AliasFrame frame = std::move(frames.back());
        frames.pop_back();
        node = frame.node;
        namespaceName = std::move(frame.namespaceName);
        scope = std::move(frame.scope);
        if (node == nullptr) continue;
    std::string currentNamespace = namespaceName;
    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
        currentNamespace = namespaceAppend(namespaceName, node->text());

    GenericLookupScope currentScope = scope;
    if (node->kind() == ast::AstNodeKind::CompilationUnit ||
        node->kind() == ast::AstNodeKind::NamespaceDeclaration)
        currentScope.declarationContainer = node;
    for (const ast::GenericParameterSyntax& parameter : node->genericParameterSyntaxes())
        if (parameter.kind == ast::GenericParameterSyntaxKind::Type ||
            parameter.kind == ast::GenericParameterSyntaxKind::TypePack)
            currentScope.typeParameters.write().insert(parameter.name);
    for (const ast::AstNode* child : node->children())
    {
        if (child == nullptr || child->kind() != ast::AstNodeKind::UsingAliasDirective) continue;
        if (!child->genericParameters().empty() || child->auxiliaryText().empty())
            currentScope.typeAliases.write()[child->text()] = child;
    }

    const bool nodeOk = expandAliasesForNode(unit, node, currentScope, diagnostics);
    ok = nodeOk && ok;
    const std::vector<ast::AstNode*> children(node->children().begin(), node->children().end());
    for (auto iterator = children.rbegin(); iterator != children.rend(); ++iterator)
        frames.push_back(AliasFrame{*iterator, currentNamespace, currentScope});
    if (node->third() != nullptr)
        frames.push_back(AliasFrame{node->third(), currentNamespace, currentScope});
    if (node->second() != nullptr)
        frames.push_back(AliasFrame{node->second(), currentNamespace, currentScope});
    if (node->first() != nullptr)
        frames.push_back(AliasFrame{node->first(), currentNamespace, currentScope});
    }
    return ok;
}
}
