#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "../Internal/CloneDepthGuard.h"
#include "../Cache/CachePayloadReader.h"
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
using internal::CachePayloadReader;
using internal::EnumConstResolver;
using internal::MirCacheRecord;
using internal::SpecializationWaveTimer;
#if 0 // INSTRUMENTATION CODE
using internal::genericNodeDetail;
using internal::genericTypeDetail;
using internal::genericUnitDetail;
using internal::GenericWorkProgress;
#endif // INSTRUMENTATION CODE
void ProjectGenericModel::addDependencyImportedNamespace(
    GenericLookupScope& scope,
    std::string candidate) const
{
    auto& imported = scope.importedNamespaces.write();
    if (std::find(imported.begin(), imported.end(), candidate) == imported.end())
        imported.push_back(std::move(candidate));
}

void ProjectGenericModel::collectDependencyImports(
    const ast::AstNode* node,
    const std::string& currentNamespace,
    GenericLookupScope& scope) const
{
    for (const ast::AstNode* child : node->children())
    {
        if (child == nullptr) continue;
        if (child->kind() == ast::AstNodeKind::UsingNamespaceDirective)
        {
            const std::string written = child->text();
            if (written.empty()) continue;
            const std::vector<std::string> segments = splitDotted(currentNamespace);
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
                addDependencyImportedNamespace(scope, std::move(prefixed));
            }
            addDependencyImportedNamespace(scope, written);
        }
        else if (child->kind() == ast::AstNodeKind::UsingAliasDirective &&
                 !child->auxiliaryText().empty())
        {
            scope.aliases.write()[child->text()] = child->auxiliaryText();
        }
    }
}

void ProjectGenericModel::discoverTemplateDependencies(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    std::string namespaceName,
    std::string ownerName,
    GenericLookupScope scope)
{
    // NSC305-G05. Iterative pre-order traversal with an explicit stack; the per-node logic is
    // unchanged, only the recursion is removed.
    std::vector<DependencyFrame> stack;
    stack.push_back(DependencyFrame{node, std::move(namespaceName), std::move(ownerName),
        std::move(scope)});
    while (!stack.empty())
    {
        DependencyFrame frame = std::move(stack.back());
        stack.pop_back();
        node = frame.node;
        namespaceName = std::move(frame.namespaceName);
        ownerName = std::move(frame.ownerName);
        scope = std::move(frame.scope);
        if (node == nullptr) continue;

    std::string currentNamespace = namespaceName;
    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
        currentNamespace = namespaceAppend(namespaceName, node->text());

    GenericLookupScope currentScope = scope;
    collectDependencyImports(node, currentNamespace, currentScope);

    if (TemplateState* root = stateForSyntax(node); root != nullptr)
    {
        const std::string templateOwner = root->record.kind == GenericTemplateKind::Method
            ? ownerName
            : appendOwner(ownerName, node->text());
        collectTemplateDependencies(
            *root, unit, node, currentNamespace, templateOwner, currentScope, node);
        continue;
    }

    const std::string childOwner =
        (node->kind() == ast::AstNodeKind::ClassDeclaration ||
         node->kind() == ast::AstNodeKind::StructDeclaration)
            ? appendOwner(ownerName, node->text())
            : ownerName;
    const std::vector<ast::AstNode*> children(node->children().begin(), node->children().end());
    for (auto iterator = children.rbegin(); iterator != children.rend(); ++iterator)
        stack.push_back(DependencyFrame{*iterator, currentNamespace, childOwner, currentScope});
    }
}

void ProjectGenericModel::collectTypeDependency(
    TemplateState& root,
    ProjectSyntaxUnit& unit,
    const ast::TypeSyntax* syntax,
    const std::string& namespaceName,
    const GenericLookupScope& scope)
{
    // NSC305-G05. Iterative traversal of the type-argument tree. The original recursed into
    // arguments first; because dependencies, unresolved dependencies and external references are
    // sorted and deduped later in build(), the visit order does not affect the final result, so a
    // plain stack suffices.
    std::vector<const ast::TypeSyntax*> stack;
    stack.push_back(syntax);
    while (!stack.empty())
    {
        const ast::TypeSyntax* current = stack.back();
        stack.pop_back();
        if (current == nullptr) continue;
        for (const ast::TypeSyntax* argument : current->typeArguments())
            stack.push_back(argument);
        // Future/Iterator/AsyncIterator, tuples, and portable SIMD are structural surfaces. Their
        // arguments may depend on nominal templates, but the surface itself has no template record
        // that a reusable NSMETA closure could or should include.
        if (isStructuralGenericSurface(current)) continue;
        if (current->typeArguments().empty())
        {
            if (const DeclaredTypeIdentity* declaration = resolveDeclaredTypeIdentity(
                    current, unit.projectOrdinal(), namespaceName, scope);
                declaration != nullptr &&
                !declaration->identityKey.starts_with(root.record.moduleName + "::"))
            {
                root.record.externalTypeReferences.push_back(
                    {current->range(), "type:" + declaration->identityKey});
            }
            continue;
        }

        TypeResolution resolution = resolveTypeTemplate(
            unit, namespaceName, current->baseName(), current->typeArguments().size());
        if (resolution.kind != TypeResolutionKind::Unique)
            resolution = resolveTypeTemplate(
                unit, namespaceName, current->baseName(),
                std::numeric_limits<std::size_t>::max());
        TemplateState* target =
            resolution.kind == TypeResolutionKind::Unique ? resolution.state : nullptr;
        if (target == nullptr)
        {
            root.record.unresolvedDependencies.push_back(
                "generic type '" + current->baseName() + "<...>'");
            continue;
        }
        if (target->record.moduleName != root.record.moduleName)
        {
            root.record.externalTypeReferences.push_back({
                current->range(),
                "type:" + target->record.canonicalKey});
        }
        root.record.dependencies.push_back(target->record.canonicalKey);
    }
}

void ProjectGenericModel::collectCallDependency(
    TemplateState& root,
    ProjectSyntaxUnit& unit,
    const ast::AstNode* call,
    const std::string& namespaceName,
    const std::string& ownerName,
    const GenericLookupScope& scope)
{
    if (call == nullptr || call->kind() != ast::AstNodeKind::CallExpression ||
        call->typeSyntaxes().empty()) return;

    const ast::AstNode* callee = call->first();
    const std::string methodName = methodCalleeName(call);
    std::string requestedOwner = templateSourceSpelling(namespaceName, ownerName);
    if (callee != nullptr && callee->kind() == ast::AstNodeKind::MemberExpression)
    {
        const std::string written = receiverSourceSpelling(callee->first());
        if (!written.empty()) requestedOwner = written;
    }

    QualifiedNameSiteContext siteContext = makeQualifiedNameSite(namespaceName, ownerName, scope);
    const QualifiedNameSite& site = siteContext.site;

    const std::size_t arity = call->typeSyntaxes().size();
    const std::size_t argumentCount = call->children().size();
    // NSC305-G03. Build the name ladder once; every arity/argument-count variation reuses it.
    const std::vector<QualifiedNameRung> ladder = qualifiedNameLadder(site, requestedOwner);
    const std::vector<TemplateState*> named = templatesNamedBy(
        unit, site, requestedOwner, methodName, arity, argumentCount, &ladder);
    const std::vector<TemplateState*> targets = collectMethodTemplatesFromNamed(named);
    if (!targets.empty())
    {
        for (const TemplateState* target : targets)
            root.record.dependencies.push_back(target->record.canonicalKey);
        return;
    }

    TemplateState* fallback = resolveMethodTemplate(
        unit, site, requestedOwner, methodName, arity, argumentCount, &named, &ladder);
    if (fallback == nullptr)
        fallback = resolveMethodTemplate(
            unit, site, requestedOwner, methodName,
            std::numeric_limits<std::size_t>::max(), argumentCount, nullptr, &ladder);
    if (fallback != nullptr)
    {
        root.record.dependencies.push_back(fallback->record.canonicalKey);
        return;
    }

    root.record.unresolvedDependencies.push_back(
        std::string(named.size() > 1U ? "ambiguous generic method '" : "missing generic method '") +
        methodName + "<...>'");
}

void ProjectGenericModel::collectBodyDependencyImports(
    const ast::AstNode* node,
    GenericLookupScope& scope) const
{
    for (const ast::AstNode* child : node->children())
    {
        if (child == nullptr) continue;
        if (child->kind() == ast::AstNodeKind::UsingNamespaceDirective)
            scope.importedNamespaces.write().push_back(child->text());
        else if (child->kind() == ast::AstNodeKind::UsingAliasDirective &&
                 !child->auxiliaryText().empty())
            scope.aliases.write()[child->text()] = child->auxiliaryText();
    }
}

void ProjectGenericModel::collectTemplateDependencies(
    TemplateState& root,
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    std::string namespaceName,
    std::string ownerName,
    GenericLookupScope scope,
    const ast::AstNode* rootSyntax)
{
    // NSC305-G05. Iterative pre-order traversal with an explicit stack; the per-node logic is
    // unchanged, only the recursion into first/second/third/children is removed.
    std::vector<BodyFrame> stack;
    stack.push_back(
        BodyFrame{node, std::move(namespaceName), std::move(ownerName), std::move(scope)});
    while (!stack.empty())
    {
        BodyFrame frame = std::move(stack.back());
        stack.pop_back();
        node = frame.node;
        namespaceName = std::move(frame.namespaceName);
        ownerName = std::move(frame.ownerName);
        scope = std::move(frame.scope);
        if (node == nullptr) continue;
        if (node != rootSyntax && stateForSyntax(node) != nullptr) continue;

    std::string currentNamespace = namespaceName;
    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
        currentNamespace = namespaceAppend(namespaceName, node->text());

    GenericLookupScope currentScope = scope;
    collectBodyDependencyImports(node, currentScope);

    collectTypeDependency(root, unit, node->typeSyntax(), currentNamespace, currentScope);
    for (const ast::TypeSyntax* syntax : node->typeSyntaxes())
        collectTypeDependency(root, unit, syntax, currentNamespace, currentScope);
    for (const ast::GenericConstraintClause& clause : node->genericConstraints())
    {
        for (const ast::TypeSyntax* constraint : clause.typeConstraints)
            collectTypeDependency(root, unit, constraint, currentNamespace, currentScope);
        collectTypeDependency(
            root, unit, clause.equalityConstraint, currentNamespace, currentScope);
        collectTypeDependency(
            root, unit, clause.substitutedArgument, currentNamespace, currentScope);
    }
    collectCallDependency(
        root, unit, node, currentNamespace, ownerName, currentScope);

    const std::string childOwner = node == rootSyntax
        ? ownerName
        : ((node->kind() == ast::AstNodeKind::ClassDeclaration ||
            node->kind() == ast::AstNodeKind::StructDeclaration)
               ? appendOwner(ownerName, node->text())
               : ownerName);
    const std::vector<ast::AstNode*> children(node->children().begin(), node->children().end());
    for (auto iterator = children.rbegin(); iterator != children.rend(); ++iterator)
        stack.push_back(BodyFrame{*iterator, currentNamespace, childOwner, currentScope});
    if (node->third() != nullptr)
        stack.push_back(BodyFrame{node->third(), currentNamespace, ownerName, currentScope});
    if (node->second() != nullptr)
        stack.push_back(BodyFrame{node->second(), currentNamespace, ownerName, currentScope});
    if (node->first() != nullptr)
        stack.push_back(BodyFrame{node->first(), currentNamespace, ownerName, currentScope});
    }
}

bool ProjectGenericModel::templateVisible(
    std::size_t from,
    const GenericTemplateRecord& target) const noexcept
{
    // A materialization-only declaration remains usable while walking a clone owned by its
    // producer project. A different project may discover only the producer's public roots.
    return from == target.projectOrdinal ||
        (target.accessibility == GenericTemplateAccessibility::Public &&
         projectVisible(from, target.projectOrdinal));
}
}
