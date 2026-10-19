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
// NSC305-G08. Builds the lexical scope for one node (see the header).
void ProjectGenericModel::prepareNodeScope(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    const std::string& namespaceName,
    const GenericLookupScope& scope,
    std::string& outNamespace,
    GenericLookupScope& outScope)
{
    outNamespace = namespaceName;
    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
    {
        outNamespace = namespaceAppend(namespaceName, node->text());
    }
    telemetry_.noteScopeCopy(
        scope.importedNamespaces.read().size() + scope.aliases.read().size() +
        scope.typeAliases.read().size() + scope.typeParameters.read().size() +
        scope.valuePassKinds.read().size() + scope.genericLambdas.read().size());
    outScope = scope;
    if (node->kind() == ast::AstNodeKind::LambdaExpression &&
        node->text().starts_with("generic-lambda-instance:"))
    {
        outScope.genericLambdas.write().erase(
            node->text().substr(std::string_view("generic-lambda-instance:").size()));
    }
    for (const ast::AstNode* child : node->children())
    {
        if (child == nullptr) continue;
        if (child->kind() == ast::AstNodeKind::UsingNamespaceDirective)
        {
            const std::string written = child->text();
            if (written.empty()) continue;
            const auto add = [&](std::string candidate)
            {
                auto& imported = outScope.importedNamespaces.write();
                if (std::find(imported.begin(), imported.end(), candidate) == imported.end())
                    imported.push_back(std::move(candidate));
            };
            const std::vector<std::string> segments = splitDotted(outNamespace);
            for (std::size_t depth = segments.size(); depth > 0; --depth)
            {
                std::string prefixed;
                for (std::size_t index = 0; index < depth; ++index)
                {
                    if (!prefixed.empty()) prefixed.push_back('.');
                    prefixed += segments[index];
                }
                prefixed.push_back('.');
                prefixed += written;
                add(std::move(prefixed));
            }
            add(written);
        }
        else if (child->kind() == ast::AstNodeKind::UsingAliasDirective)
        {
            if (!child->auxiliaryText().empty())
            {
                outScope.aliases.write()[child->text()] = child->auxiliaryText();
            }
        }
    }
    if ((node->kind() == ast::AstNodeKind::MethodDeclaration ||
         node->kind() == ast::AstNodeKind::FunctionDeclaration) &&
        node->typeSyntax() != nullptr)
    {
        bool conflict = false;
        outScope.expectedReturnType =
            cloneAliasType(unit.arena(), node->typeSyntax(), {}, conflict);
    }
    for (const ast::AstNode* child : node->children())
    {
        if (child == nullptr || child->typeSyntax() == nullptr ||
            (child->kind() != ast::AstNodeKind::ParameterDeclaration &&
             child->kind() != ast::AstNodeKind::FieldDeclaration))
            continue;
        bool conflict = false;
        outScope.addValueType(child->text(),
            cloneAliasType(unit.arena(), child->typeSyntax(), {}, conflict));
        if (child->kind() == ast::AstNodeKind::ParameterDeclaration)
        {
            outScope.valuePassKinds.write().erase(child->text());
            if (child->auxiliaryText().starts_with("pass:"))
            {
                const std::size_t end = child->auxiliaryText().find('|');
                outScope.valuePassKinds.write()[child->text()] =
                    child->auxiliaryText().substr(5U, end == std::string::npos ? end : end - 5U);
            }
        }
    }
}

// NSC305-G08. The per-node body of processNode (see the header). Returns false when the node's
// children must be skipped (alias directive or open template).
void ProjectGenericModel::prepareMatchExpressionScope(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    GenericLookupScope& scope)
{
    // MatchExpression arm conditions and bindings use MatchValueExpression to read the one-time
    // governing value. Preserve its structural source type lexically so body-only demands can
    // materialize closed view types before Sema. Match statements expose an ordinary hidden local.
    if (node->kind() == ast::AstNodeKind::MatchExpression && node->first() != nullptr)
    {
        scope.matchValuePassKind.clear();
        if (ast::TypeSyntax* governingType = inferExpressionType(unit, node->first(), scope))
            scope.matchValueType = governingType;
        if (node->first()->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const auto pass = scope.valuePassKinds.read().find(node->first()->text());
            if (pass != scope.valuePassKinds.read().end())
                scope.matchValuePassKind = pass->second;
        }
    }
}

void ProjectGenericModel::processNodeGenericConstraints(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    const std::string& currentNamespace,
    GenericLookupScope& currentScope,
    diagnostics::DiagnosticEngine& diagnostics)
{
    for (const ast::GenericConstraintClause& clause : node->genericConstraints())
    {
        for (const ast::TypeSyntax* constraint : clause.typeConstraints)
            processTypeSyntax(
                unit, const_cast<ast::TypeSyntax*>(constraint), currentNamespace, currentScope,
                diagnostics);
        std::string substitutedConstantType;
        std::string substitutedConstantValue;
        if (clause.substitutedArgument != nullptr &&
            !language::parseConstGenericArgumentMarker(
                clause.substitutedArgument->baseName(),
                substitutedConstantType,
                substitutedConstantValue))
        {
            processTypeSyntax(
                unit, const_cast<ast::TypeSyntax*>(clause.substitutedArgument), currentNamespace,
                currentScope, diagnostics);
        }
    }
}

bool ProjectGenericModel::shouldSkipTemplateNode(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    const std::string& currentNamespace,
    GenericLookupScope& currentScope,
    bool templateNode,
    bool insideTemplate,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (templateNode || insideTemplate)
    {
        if (isGenericMethodTemplateNode(node))
        {
            GenericLookupScope receiverScope = currentScope;
            for (const std::string& parameter : node->genericParameters())
                receiverScope.typeParameters.write().insert(parameter);
            for (const ast::AstNode* child : node->children())
            {
                if (child == nullptr ||
                    child->kind() != ast::AstNodeKind::ParameterDeclaration ||
                    child->typeSyntax() == nullptr)
                {
                    continue;
                }
                processTypeSyntax(
                    unit, const_cast<ast::TypeSyntax*>(child->typeSyntax()), currentNamespace,
                    receiverScope, diagnostics);
                break;
            }
        }
        return true;
    }
    return false;
}

ast::TypeSyntax* ProjectGenericModel::declarationExpectedType(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* node)
{
    if (node->kind() == ast::AstNodeKind::VariableDeclarationStatement &&
        node->typeSyntax() != nullptr &&
        node->typeSyntax()->baseTokenKind() != lexer::TokenKind::KwVar)
    {
        bool conflict = false;
        return cloneAliasType(unit.arena(), node->typeSyntax(), {}, conflict);
    }
    return nullptr;
}

void ProjectGenericModel::processNodePrimaryTypeSyntax(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    const std::string& currentNamespace,
    GenericLookupScope& currentScope,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (node->kind() == ast::AstNodeKind::NameOfExpression &&
        node->typeSyntax() != nullptr && !node->typeSyntax()->typeArguments().empty())
    {
        ast::TypeSyntax* candidate = node->typeSyntax();
        TypeResolution typeResolution = resolveTypeTemplate(
            unit, currentNamespace, candidate->baseName(), candidate->typeArguments().size());
        if (typeResolution.kind != TypeResolutionKind::Unique)
        {
            typeResolution = resolveTypeTemplate(
                unit, currentNamespace, candidate->baseName(),
                std::numeric_limits<std::size_t>::max());
        }
        if (typeResolution.kind == TypeResolutionKind::Unique)
        {
            processTypeSyntax(unit, candidate, currentNamespace, currentScope, diagnostics);
        }
        else
        {
            for (ast::TypeSyntax* argument : candidate->typeArguments())
                processTypeSyntax(unit, argument, currentNamespace, currentScope, diagnostics);
        }
    }
    else if (node->kind() == ast::AstNodeKind::ExtensionDeclaration && node->typeSyntax() != nullptr)
    {
        GenericLookupScope extensionScope = currentScope;
        for (const std::string& parameter : node->genericParameters())
            extensionScope.typeParameters.write().insert(parameter);
        processTypeSyntax(unit, node->typeSyntax(), currentNamespace, extensionScope, diagnostics);
    }
    else if (node->typeSyntax() != nullptr)
    {
        processTypeSyntax(unit, node->typeSyntax(), currentNamespace, currentScope, diagnostics);
    }
}

void ProjectGenericModel::processNodeGenericCalls(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    const std::string& currentNamespace,
    const std::string& ownerName,
    GenericLookupScope& currentScope,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool specializedGenericLambda = false;
    if (node->kind() == ast::AstNodeKind::CallExpression && node->first() != nullptr &&
        node->first()->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        const auto local = currentScope.genericLambdas.read().find(node->first()->text());
        if (local != currentScope.genericLambdas.read().end())
            specializedGenericLambda = specializeGenericLambdaCall(
                unit, node, local->second, currentScope, diagnostics);
    }
    if (!specializedGenericLambda && node->kind() == ast::AstNodeKind::CallExpression &&
        node->typeSyntaxes().empty() && !node->flag())
    {
        inferGenericCall(
            unit, node, currentNamespace, ownerName, currentScope, diagnostics);
    }
    if (!specializedGenericLambda && node->kind() == ast::AstNodeKind::CallExpression &&
        (!node->typeSyntaxes().empty() || node->flag()))
    {
        processGenericCall(unit, node, currentNamespace, ownerName, currentScope, diagnostics);
    }
    else
    {
        for (ast::TypeSyntax* syntax : node->typeSyntaxes())
        {
            processTypeSyntax(unit, syntax, currentNamespace, currentScope, diagnostics);
        }
    }
}

void ProjectGenericModel::applyNodeExpectedType(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    const ast::TypeSyntax* expectedDeclarationType,
    const std::string& currentNamespace,
    const std::string& ownerName,
    GenericLookupScope& currentScope,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // Bidirectional inference is deliberately bounded to contexts with one exact expected type.
    ast::AstNode* contextualCall = nullptr;
    const ast::TypeSyntax* contextualType = nullptr;
    if (node->kind() == ast::AstNodeKind::VariableDeclarationStatement)
    {
        contextualCall = node->first();
        contextualType = expectedDeclarationType;
    }
    else if (node->kind() == ast::AstNodeKind::ReturnStatement)
    {
        contextualCall = node->first();
        contextualType = currentScope.expectedReturnType;
    }
    else if (node->kind() == ast::AstNodeKind::AssignmentExpression)
    {
        contextualCall = node->second();
        contextualType = inferExpressionType(unit, node->first(), currentScope);
    }
    if (contextualCall != nullptr && contextualType != nullptr)
        applyExpectedTypeToExpression(
            unit, contextualCall, contextualType, currentNamespace, ownerName, currentScope,
            diagnostics);
}

bool ProjectGenericModel::processNodeOnce(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    const std::string& currentNamespace,
    const std::string& ownerName,
    GenericLookupScope& currentScope,
    bool insideTemplate,
    diagnostics::DiagnosticEngine& diagnostics)
{
    prepareMatchExpressionScope(unit, node, currentScope);
    // The declaration is a compile-time rewrite rule, not a use of its still-open target.
    if (node->kind() == ast::AstNodeKind::UsingAliasDirective) return false;
    const bool genericLambdaTemplate = node->kind() == ast::AstNodeKind::LambdaExpression &&
        !node->genericParameters().empty();
    const bool templateNode = isGenericTypeTemplateNode(node) ||
        isGenericMethodTemplateNode(node) || genericLambdaTemplate;
    processNodeGenericConstraints(unit, node, currentNamespace, currentScope, diagnostics);
    if (shouldSkipTemplateNode(
            unit,
            node,
            currentNamespace,
            currentScope,
            templateNode,
            insideTemplate,
            diagnostics))
        return false;
    ast::TypeSyntax* expectedType = declarationExpectedType(unit, node);
    processNodePrimaryTypeSyntax(unit, node, currentNamespace, currentScope, diagnostics);
    collectBodyTypeSpecializationDemand(unit, node, currentNamespace, currentScope);
    processNodeGenericCalls(
        unit, node, currentNamespace, ownerName, currentScope, diagnostics);
    applyNodeExpectedType(
        unit, node, expectedType, currentNamespace, ownerName, currentScope, diagnostics);
    return true;
}

void ProjectGenericModel::scheduleProcessedNodeChildren(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    const std::string& currentNamespace,
    const std::string& ownerName,
    const GenericLookupScope& currentScope,
    std::vector<ProcessFrame>& frames)
{
    // NSC-LIMIT-009: se APILA. La version anterior reemplazaba, de modo que al entrar en
    // `A { Inner { ... } }` el dueno pasaba a ser «Inner» y `A` desaparecia.
    const std::string childOwner =
        (node->kind() == ast::AstNodeKind::ClassDeclaration ||
         node->kind() == ast::AstNodeKind::StructDeclaration)
            ? appendOwner(ownerName, node->text()) : ownerName;
    const std::vector<ast::AstNode*> children(node->children().begin(), node->children().end());
    // NSC305-G08. Pre-compute each child's scope snapshot (the sequential local accumulation),
    // then push children in reverse so the stack pops them in source order.
    std::vector<GenericLookupScope> childScopes;
    childScopes.reserve(children.size());
    GenericLookupScope sequentialScope = currentScope;
    for (ast::AstNode* child : children)
    {
        ast::TypeSyntax* declaredLocalType = nullptr;
        if (child != nullptr && child->kind() == ast::AstNodeKind::VariableDeclarationStatement &&
            child->typeSyntax() != nullptr &&
            child->typeSyntax()->baseTokenKind() != lexer::TokenKind::KwVar)
        {
            bool conflict = false;
            declaredLocalType =
                cloneAliasType(unit.arena(), child->typeSyntax(), {}, conflict);
        }
        else if (child != nullptr &&
                 child->kind() == ast::AstNodeKind::VariableDeclarationStatement &&
                 child->first() != nullptr)
        {
            // Capture `var` from the initializer before its closed generic syntax is rewritten to
            // a physical specialization name; structural inference still needs Box<int>, not the
            // generated Box__g1__... spelling.
            declaredLocalType = inferExpressionType(unit, child->first(), sequentialScope);
        }
        childScopes.push_back(sequentialScope);
        if (child == nullptr || child->kind() != ast::AstNodeKind::VariableDeclarationStatement)
            continue;
        const ast::AstNode* initializer = child->first();
        if (initializer != nullptr && initializer->kind() == ast::AstNodeKind::LambdaExpression &&
            !initializer->genericParameters().empty())
        {
            sequentialScope.genericLambdas.write()[child->text()] =
                GenericLookupScope::GenericLambda{
                    initializer, child, sequentialScope.declarationContainer};
            continue;
        }
        if (declaredLocalType == nullptr && child->first() != nullptr)
            declaredLocalType = inferExpressionType(unit, child->first(), sequentialScope);
        if (declaredLocalType != nullptr)
            sequentialScope.addValueType(child->text(), declaredLocalType);
    }
    for (std::size_t index = children.size(); index > 0; --index)
        frames.push_back(ProcessFrame{children[index - 1U], currentNamespace, childOwner,
            childScopes[index - 1U], false});
    if (node->third() != nullptr)
        frames.push_back(ProcessFrame{node->third(), currentNamespace, ownerName, currentScope, false});
    if (node->second() != nullptr)
        frames.push_back(ProcessFrame{node->second(), currentNamespace, ownerName, currentScope, false});
    if (node->first() != nullptr)
        frames.push_back(ProcessFrame{node->first(), currentNamespace, ownerName, currentScope, false});
}

void ProjectGenericModel::processNode(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    std::string namespaceName,
    std::string ownerName,
    GenericLookupScope scope,
    bool insideTemplate,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // NSC305-G08. Iterative pre-order traversal with an explicit stack; the per-node logic is
    // unchanged, only the recursion into first/second/third/children is removed. The sequential
    // local accumulation is preserved by computing each child's scope snapshot before the walk.
    std::vector<ProcessFrame> frames;
    frames.push_back(
        ProcessFrame{
            node, std::move(namespaceName), std::move(ownerName),
            std::move(scope), insideTemplate});
    while (!frames.empty())
    {
        ProcessFrame frame = std::move(frames.back());
        frames.pop_back();
        node = frame.node;
        namespaceName = std::move(frame.namespaceName);
        ownerName = std::move(frame.ownerName);
        scope = std::move(frame.scope);
        insideTemplate = frame.insideTemplate;
        if (node == nullptr) continue;

#if 0 // INSTRUMENTATION CODE
        const GenericWorkProgress nodeProgress(
            telemetry_, "generics.root-demand-node", genericNodeDetail(unit, *node));
#endif // INSTRUMENTATION CODE

    std::string currentNamespace;
    GenericLookupScope currentScope;
    prepareNodeScope(unit, node, namespaceName, scope, currentNamespace, currentScope);

    if (!processNodeOnce(
            unit, node, currentNamespace, ownerName, currentScope, insideTemplate, diagnostics))
        continue;

    scheduleProcessedNodeChildren(
        unit, node, currentNamespace, ownerName, currentScope, frames);
    }
}
}
