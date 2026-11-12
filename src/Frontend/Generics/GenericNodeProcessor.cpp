#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
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
#include "StaticFieldTemplateRewriter.h"
#include "StaticFieldTypeValidation.h"
#include "StaticFieldControlFlow.h"
#include "nativesharp/Source/SourceFile.h"
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
    if (node->kind() == ast::AstNodeKind::MethodDeclaration ||
        node->kind() == ast::AstNodeKind::FunctionDeclaration ||
        node->kind() == ast::AstNodeKind::LambdaExpression)
        outScope.activeLoopLabels.write().clear();
    if (node->kind() == ast::AstNodeKind::ClassDeclaration ||
        node->kind() == ast::AstNodeKind::StructDeclaration)
        outScope.lexicalTypeSyntax = node;
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
    GenericLookupScope constraintScope = currentScope;
    for (const std::string& parameter : node->genericParameters())
        constraintScope.typeParameters.write().insert(parameter);
    for (const ast::GenericParameterSyntax& parameter : node->genericParameterSyntaxes())
        if (parameter.kind == ast::GenericParameterSyntaxKind::Constant ||
            parameter.kind == ast::GenericParameterSyntaxKind::ConstantPack)
            constraintScope.constParameters.write().insert(parameter.name);
    for (const ast::GenericConstraintClause& clause : node->genericConstraints())
    {
        for (const ast::TypeSyntax* constraint : clause.typeConstraints)
            processTypeSyntax(
                unit, const_cast<ast::TypeSyntax*>(constraint), currentNamespace, constraintScope,
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
                constraintScope, diagnostics);
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
        if (isGenericTypeTemplateNode(node))
        {
            GenericLookupScope ownerScope = currentScope;
            for (const std::string& parameter : node->genericParameters())
                ownerScope.typeParameters.write().insert(parameter);
            for (const ast::GenericParameterSyntax& parameter : node->genericParameterSyntaxes())
                if (parameter.kind == ast::GenericParameterSyntaxKind::Constant ||
                    parameter.kind == ast::GenericParameterSyntaxKind::ConstantPack)
                    ownerScope.constParameters.write().insert(parameter.name);
            for (ast::AstNode* child : node->children())
                if (child != nullptr && isGenericMethodTemplateNode(child))
                    processNodeGenericConstraints(unit, child, currentNamespace, ownerScope, diagnostics);
        }
        if (isGenericMethodTemplateNode(node))
        {
            GenericLookupScope receiverScope = currentScope;
            for (const std::string& parameter : node->genericParameters())
                receiverScope.typeParameters.write().insert(parameter);
            for (const ast::GenericParameterSyntax& parameter : node->genericParameterSyntaxes())
            {
                if (parameter.kind == ast::GenericParameterSyntaxKind::Constant ||
                    parameter.kind == ast::GenericParameterSyntaxKind::ConstantPack)
                    receiverScope.constParameters.write().insert(parameter.name);
            }
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
            unit, currentNamespace, candidate->baseName(), candidate->typeArguments().size(), currentScope);
        if (typeResolution.kind != TypeResolutionKind::Unique)
        {
            typeResolution = resolveTypeTemplate(
                unit, currentNamespace, candidate->baseName(),
                std::numeric_limits<std::size_t>::max(), currentScope);
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

static bool matchingStaticFieldType(const ast::TypeSyntax* left, const ast::TypeSyntax* right,
    const std::vector<std::string>& parameters, std::size_t depth = 0)
{
    if (left == nullptr || right == nullptr || depth >= 64 ||
        left->baseTokenKind() != right->baseTokenKind() || left->baseName() != right->baseName() ||
        left->isTuple() != right->isTuple() || left->tupleElementNames() != right->tupleElementNames() ||
        left->isFunction() || right->isFunction() || left->isDynamicTrait() || right->isDynamicTrait() ||
        left->isOpaqueResult() || right->isOpaqueResult() || left->isPackExpansion() || right->isPackExpansion() ||
        left->isPointeeConst() || right->isPointeeConst() ||
        left->constExpression() != nullptr || right->constExpression() != nullptr ||
        !left->lifetimeName().empty() || !right->lifetimeName().empty() ||
        left->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None ||
        right->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None ||
        left->referenceQualifier() != ast::ReferenceQualifierSyntaxKind::None ||
        right->referenceQualifier() != ast::ReferenceQualifierSyntaxKind::None ||
        left->modifiers().size() != right->modifiers().size() ||
        left->typeArguments().size() != right->typeArguments().size()) return false;
    for (std::size_t i = 0; i < left->modifiers().size(); ++i)
    {
        const auto& a = left->modifiers()[i];
        const auto& b = right->modifiers()[i];
        if (a.kind != ast::TypeModifierSyntaxKind::Pointer || a.kind != b.kind ||
            a.arrayCount != nullptr || b.arrayCount != nullptr || a.arrayRank != b.arrayRank ||
            a.pointerNullability != b.pointerNullability || a.pointerHasProvenance != b.pointerHasProvenance ||
            a.pointerKnownAlignment != b.pointerKnownAlignment || a.pointerAddressSpace != b.pointerAddressSpace)
            return false;
    }
    if (!left->isTuple())
    {
        if (!left->typeArguments().empty()) return false;
        return lexer::isBuiltinTypeKeyword(left->baseTokenKind())
            ? left->baseTokenKind() != lexer::TokenKind::KwVoid
            : left->baseTokenKind() == lexer::TokenKind::Identifier &&
                std::find(parameters.begin(), parameters.end(), left->baseName()) != parameters.end();
    }
    if (left->typeArguments().empty()) return false;
    for (std::size_t i = 0; i < left->typeArguments().size(); ++i)
        if (!matchingStaticFieldType(left->typeArguments()[i], right->typeArguments()[i], parameters, depth + 1))
            return false;
    return true;
}

static bool verifiedSourceBuiltinTuple(const ast::TypeSyntax* type)
{
    if (type == nullptr || !type->isTuple() || !matchingStaticFieldType(type, type, {})) return false;
    std::vector<const ast::TypeSyntax*> pending{type};
    while (!pending.empty())
    {
        const ast::TypeSyntax* current = pending.back();
        pending.pop_back();
        if (current == nullptr || !current->modifiers().empty()) return false;
        for (const ast::TypeSyntax* argument : current->typeArguments()) pending.push_back(argument);
    }
    return true;
}

static bool verifiedSourceConstantIntArray(const ast::TypeSyntax* type)
{
    if (type == nullptr || type->baseTokenKind() != lexer::TokenKind::KwInt ||
        type->isTuple() || type->isFunction() || type->isDynamicTrait() || type->isOpaqueResult() ||
        type->isPackExpansion() || type->isPointeeConst() || type->isExplicitTraitImplementation() ||
        type->constExpression() != nullptr || !type->lifetimeName().empty() || !type->typeArguments().empty() ||
        type->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None ||
        type->referenceQualifier() != ast::ReferenceQualifierSyntaxKind::None || type->modifiers().size() != 1)
        return false;
    const ast::TypeModifierSyntax& modifier = type->modifiers()[0];
    if (modifier.kind != ast::TypeModifierSyntaxKind::Array || modifier.arrayCount == nullptr) return false;
    std::vector<std::pair<const ast::AstNode*, std::size_t>> work{{modifier.arrayCount, 0}};
    std::size_t visits = 0;
    while (!work.empty())
    {
        const auto [node, depth] = work.back();
        work.pop_back();
        if (node == nullptr || depth >= 64 || ++visits > 1024) return false;
        using K = ast::AstNodeKind;
        if (node->kind() == K::IntegerLiteralExpression || node->kind() == K::CharacterLiteralExpression) continue;
        if (node->kind() == K::UnaryExpression) work.emplace_back(node->first(), depth + 1);
        else if (node->kind() == K::BinaryExpression)
        {
            work.emplace_back(node->first(), depth + 1);
            work.emplace_back(node->second(), depth + 1);
        }
        else return false;
    }
    const auto count = ProjectMemberModel::evaluateConstantArrayCount(modifier.arrayCount);
    return count.has_value() && *count > 0;
}

// Reparse the verified metadata slice independently. Only simple field shapes
// are admitted here; a more expressive source must fail closed until compared.
static bool verifyTransportedTemplateFields(
    const PrecompiledTemplateFieldCertificate& certificate, const ast::AstNode* transported)
{
    if (transported == nullptr || certificate.sourceText.empty()) return false;
    diagnostics::DiagnosticEngine diagnostics;
    source::SourceFile source(FileId{1U}, std::filesystem::path{}, certificate.sourceText);
    ProjectSyntaxUnit unit(0, 0, "", FileId{1U});
    std::vector<std::string> knownTypeNames;
    for (const ast::TypeSyntax* base : transported->typeSyntaxes())
        if (base != nullptr && !base->baseName().empty()) knownTypeNames.push_back(base->baseName());
    // Parser hints only disambiguate syntax; the parsed base is compared below
    // and later resolved to a separately certified template identity.
    if (!unit.lex(source, diagnostics) || !unit.parse(diagnostics, knownTypeNames) || diagnostics.hasErrors() ||
        unit.syntaxTree() == nullptr) return false;
    const auto& children = unit.syntaxTree()->children();
    if (children.size() != 1 || children[0] == nullptr) return false;
    const ast::AstNode* parsed = children[0];
    if (parsed->kind() != transported->kind() || parsed->text() != transported->text() ||
        parsed->tokenKind() != transported->tokenKind() || parsed->flag() || transported->flag() ||
        parsed->genericParameters() != transported->genericParameters() ||
        !parsed->genericConstraints().empty() || !transported->genericConstraints().empty() ||
        parsed->typeSyntaxes().size() != transported->typeSyntaxes().size() ||
        parsed->typeSyntaxes().size() > 1) return false;
    if (!parsed->typeSyntaxes().empty())
    {
        const ast::TypeSyntax* left = parsed->typeSyntaxes()[0];
        const ast::TypeSyntax* right = transported->typeSyntaxes()[0];
        if (left == nullptr || right == nullptr || left->baseTokenKind() != lexer::TokenKind::Identifier ||
            left->baseTokenKind() != right->baseTokenKind() || left->baseName() != right->baseName() ||
            left->typeArguments().size() != right->typeArguments().size() ||
            !left->modifiers().empty() || !right->modifiers().empty() ||
            left->isDynamicTrait() || right->isDynamicTrait() || left->isFunction() || right->isFunction() ||
            left->isTuple() || right->isTuple() || left->isPackExpansion() || right->isPackExpansion() ||
            left->isOpaqueResult() || right->isOpaqueResult() || left->isPointeeConst() || right->isPointeeConst() ||
            left->isExplicitTraitImplementation() || right->isExplicitTraitImplementation() ||
            left->constExpression() != nullptr || right->constExpression() != nullptr ||
            !left->lifetimeName().empty() || !right->lifetimeName().empty() ||
            left->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None ||
            right->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None ||
            left->referenceQualifier() != ast::ReferenceQualifierSyntaxKind::None ||
            right->referenceQualifier() != ast::ReferenceQualifierSyntaxKind::None)
            return false;
        for (std::size_t i = 0; i < left->typeArguments().size(); ++i)
        {
            const ast::TypeSyntax* argument = left->typeArguments()[i];
            if (argument == nullptr || argument->isTuple() || !argument->modifiers().empty() ||
                !matchingStaticFieldType(argument, right->typeArguments()[i], parsed->genericParameters()))
                return false;
        }
    }
    std::vector<const ast::AstNode*> expected;
    std::vector<const ast::AstNode*> actual;
    for (const ast::AstNode* child : parsed->children())
        if (child != nullptr && child->kind() == ast::AstNodeKind::FieldDeclaration) expected.push_back(child);
    for (const ast::AstNode* child : transported->children())
        if (child != nullptr && child->kind() == ast::AstNodeKind::FieldDeclaration) actual.push_back(child);
    if (expected.size() != actual.size()) return false;
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        const ast::AstNode* left = expected[i];
        const ast::AstNode* right = actual[i];
        const ast::TypeSyntax* leftType = left->typeSyntax();
        const ast::TypeSyntax* rightType = right->typeSyntax();
        if (left->text() != right->text() || left->tokenKind() != right->tokenKind() ||
            left->hasModifier(lexer::TokenKind::KwStatic) != right->hasModifier(lexer::TokenKind::KwStatic) ||
            left->hasModifier(lexer::TokenKind::KwConst) != right->hasModifier(lexer::TokenKind::KwConst) ||
            left->hasModifier(lexer::TokenKind::KwReadonly) || right->hasModifier(lexer::TokenKind::KwReadonly) ||
            left->hasModifier(lexer::TokenKind::KwVolatile) || right->hasModifier(lexer::TokenKind::KwVolatile) ||
            !left->auxiliaryText().empty() || !right->auxiliaryText().empty() ||
            !matchingStaticFieldType(leftType, rightType, parsed->genericParameters())) return false;
    }
    return true;
}

// Select source structs and non-generic class chains with simple builtin fields.
// Other shapes remain rejected until their early identity can be verified.
bool ProjectGenericModel::collectStaticFieldCandidates(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* statement,
    const std::string& currentNamespace,
    const std::string& ownerName,
    const GenericLookupScope& scope,
    bool projectsFieldType,
    std::unordered_set<const ast::AstNode*>& activeTypes,
    StaticFieldIterationSelection& selection,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const ast::TypeSyntax* queried = statement->typeSyntax();
    // Resolve a simple nested name in its lexical owner chain before the
    // ordinary namespace/import lookup. Ambiguous rungs fail closed.
    bool nestedAmbiguous = false;
    const auto resolveNested = [&](const ast::TypeSyntax* syntax, std::string owner,
                                   std::string_view ns, std::size_t project) -> const DeclaredTypeIdentity*
    {
        if (syntax == nullptr || syntax->baseTokenKind() != lexer::TokenKind::Identifier ||
            !syntax->typeArguments().empty() || syntax->baseName().find('.') != std::string::npos ||
            syntax->baseName().find(':') != std::string::npos) return nullptr;
        while (!owner.empty())
        {
            std::string name = qualifiedName(std::string(ns), appendOwner(owner, syntax->baseName()));
            std::replace(name.begin(), name.end(), '+', '.');
            const auto found = declaredTypeIdentitiesBySourceName_.find(name);
            const DeclaredTypeIdentity* match = nullptr;
            if (found != declaredTypeIdentitiesBySourceName_.end())
            {
                for (const std::size_t index : found->second)
                {
                    const DeclaredTypeIdentity& candidate = declaredTypeIdentities_[index];
                    if (candidate.projectOrdinal != project) continue;
                    if (match != nullptr) { nestedAmbiguous = true; return nullptr; }
                    match = &candidate;
                }
            }
            if (match != nullptr) return match;
            const std::size_t separator = owner.rfind('+');
            if (separator == std::string::npos) break;
            owner.resize(separator);
        }
        return nullptr;
    };
    const DeclaredTypeIdentity* type = resolveNested(queried, ownerName, currentNamespace, unit.projectOrdinal());
    if (type == nullptr && !nestedAmbiguous)
        type = resolveDeclaredTypeIdentity(queried, unit.projectOrdinal(), currentNamespace, scope);
    const std::string callerIdentity = std::string(unit.moduleName()) + "::" +
        qualifiedName(currentNamespace, ownerName);
    const std::string* genericIdentity = queried != nullptr ? lookupResolvedTypeIdentity(queried) : nullptr;
    if (genericIdentity != nullptr && genericIdentity->starts_with("generic:"))
    {
        const auto ordinal = specializationPlanner_.request(genericIdentity->substr(8));
        const GenericSpecializationRecord* record = ordinal.has_value() && *ordinal < specializations_.size()
            ? &specializations_[*ordinal] : nullptr;
        const TemplateState* state = record != nullptr && record->templateOrdinal < templateStates_.size()
            ? &templateStates_[record->templateOrdinal] : nullptr;
        bool verifiedOrigin = state != nullptr && state->unit != nullptr &&
            state->unit->origin() == SyntaxUnitOrigin::Source;
        if (!verifiedOrigin && state != nullptr && state->unit != nullptr &&
            state->unit->origin() == SyntaxUnitOrigin::PrecompiledMetadata &&
            state->record.moduleName == state->unit->moduleName() &&
            state->record.canonicalKey.starts_with(std::string(state->unit->moduleName()) + "::"))
        {
            const auto certificate = precompiledTemplateFieldCertificates_.find(state->record.canonicalKey);
            verifiedOrigin = certificate != precompiledTemplateFieldCertificates_.end() &&
                verifyTransportedTemplateFields(certificate->second, state->syntax);
        }
        if (record == nullptr || record->state != SpecializationState::Ready ||
            record->specializationKey != genericIdentity->substr(8) ||
            (record->kind != GenericTemplateKind::Class && record->kind != GenericTemplateKind::Struct) ||
            record->specializedSyntax == nullptr || record->templateSyntax == nullptr || state == nullptr ||
            record->templateSyntax != state->syntax ||
            !verifiedOrigin ||
            record->specializedSyntax->typeSyntaxes().size() > 1 ||
            (record->specializedSyntax->kind() != ast::AstNodeKind::StructDeclaration &&
             record->specializedSyntax->kind() != ast::AstNodeKind::ClassDeclaration))
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error, statement->range(),
                "static field iteration cannot verify the materialized generic type");
            return false;
        }
        if (activeTypes.size() >= 1024 || !activeTypes.insert(record->specializedSyntax).second)
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error, statement->range(),
                "static field iteration generic base chain is cyclic or exceeds its limit");
            return false;
        }
        selection.queriedTypeIdentity = state->record.moduleName + "::" +
            qualifiedName(state->record.namespaceName,
                          appendOwner(state->record.ownerName, record->specializedName));
        if (!record->templateSyntax->typeSyntaxes().empty() ||
            !record->specializedSyntax->typeSyntaxes().empty())
        {
            if (record->templateSyntax->typeSyntaxes().size() != 1 ||
                record->specializedSyntax->typeSyntaxes().size() != 1)
            {
                diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error, statement->range(),
                    "static field iteration cannot verify the generic base");
                return false;
            }
            const ast::TypeSyntax* sourceBase = record->templateSyntax->typeSyntaxes()[0];
            ast::TypeSyntax* clonedBase = record->specializedSyntax->typeSyntaxes()[0];
            if (clonedBase == nullptr || clonedBase->baseTokenKind() != lexer::TokenKind::Identifier ||
                !clonedBase->typeArguments().empty() || !clonedBase->modifiers().empty() ||
                clonedBase->isDynamicTrait() || clonedBase->isFunction() || clonedBase->isTuple() ||
                clonedBase->isPackExpansion() || clonedBase->isOpaqueResult() || clonedBase->isPointeeConst() ||
                clonedBase->isExplicitTraitImplementation() || clonedBase->constExpression() != nullptr ||
                !clonedBase->lifetimeName().empty() ||
                clonedBase->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None ||
                clonedBase->referenceQualifier() != ast::ReferenceQualifierSyntaxKind::None)
            {
                diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error, statement->range(),
                    "static field iteration cannot verify the cloned generic base");
                return false;
            }
            GenericLookupScope declarationScope;
            declarationScope.importedNamespaces.write() = state->record.declarationImports;
            declarationScope.aliases.write().insert(state->record.declarationAliases.begin(),
                                                    state->record.declarationAliases.end());
            ast::TypeSyntax* baseType = nullptr;
            if (sourceBase != nullptr && !sourceBase->typeArguments().empty())
            {
                const TypeResolution sourceTemplate = resolveTypeTemplate(*state->unit,
                    state->record.namespaceName, sourceBase->baseName(), sourceBase->typeArguments().size(),
                    declarationScope);
                const std::string* baseIdentity = lookupResolvedTypeIdentity(clonedBase);
                const auto baseOrdinal = baseIdentity != nullptr && baseIdentity->starts_with("generic:")
                    ? specializationPlanner_.request(baseIdentity->substr(8)) : std::nullopt;
                const GenericSpecializationRecord* baseRecord = baseOrdinal.has_value() && *baseOrdinal < specializations_.size()
                    ? &specializations_[*baseOrdinal] : nullptr;
                if (sourceTemplate.kind == TypeResolutionKind::Unique && sourceTemplate.state != nullptr &&
                    sourceTemplate.state->unit != nullptr &&
                    templateVisible(state->unit->projectOrdinal(), sourceTemplate.state->record) &&
                    (sourceTemplate.state->unit->origin() == SyntaxUnitOrigin::PrecompiledMetadata ||
                     (state->unit->origin() == SyntaxUnitOrigin::Source &&
                      sourceTemplate.state->unit->origin() == SyntaxUnitOrigin::Source &&
                      sourceTemplate.state->unit->projectOrdinal() == state->unit->projectOrdinal())) &&
                    baseRecord != nullptr && baseRecord->state == SpecializationState::Ready &&
                    baseRecord->kind == GenericTemplateKind::Class &&
                    baseRecord->templateSyntax == sourceTemplate.state->syntax &&
                    baseRecord->specializationKey == baseIdentity->substr(8)) baseType = clonedBase;
            }
            else
            {
                const DeclaredTypeIdentity* sourceIdentity = resolveDeclaredTypeIdentity(sourceBase,
                    state->unit->projectOrdinal(), state->record.namespaceName, declarationScope);
                const DeclaredTypeIdentity* clonedIdentity = resolveDeclaredTypeIdentity(clonedBase,
                    state->unit->projectOrdinal(), state->record.namespaceName, declarationScope);
                const auto baseContext = sourceIdentity != nullptr && sourceIdentity->syntax != nullptr
                    ? declaredFieldContexts_.find(sourceIdentity->syntax) : declaredFieldContexts_.end();
                const bool sourceBaseVerified = state->unit->origin() == SyntaxUnitOrigin::Source &&
                    baseContext != declaredFieldContexts_.end() && baseContext->second.unit != nullptr &&
                    baseContext->second.unit->origin() == SyntaxUnitOrigin::Source &&
                    baseContext->second.unit->projectOrdinal() == state->unit->projectOrdinal();
                if (sourceIdentity != nullptr && clonedIdentity != nullptr && sourceIdentity->syntax != nullptr &&
                    sourceIdentity->syntax == clonedIdentity->syntax &&
                    sourceIdentity->identityKey == clonedIdentity->identityKey &&
                    sourceIdentity->projectOrdinal == state->unit->projectOrdinal() &&
                    sourceIdentity->syntax->kind() == ast::AstNodeKind::ClassDeclaration &&
                    (sourceBaseVerified || precompiledFieldCertificates_.contains(sourceIdentity->identityKey)))
                {
                    std::string spelling = sourceIdentity->identityKey;
                    std::replace(spelling.begin(), spelling.end(), '+', '.');
                    baseType = unit.arena().make<ast::TypeSyntax>(
                        statement->range(), lexer::TokenKind::Identifier, std::move(spelling));
                }
            }
            if (baseType == nullptr)
            {
                diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error, statement->range(),
                    "static field iteration cannot verify the generic base identity");
                return false;
            }
            ast::AstNode* baseQuery = unit.arena().make<ast::AstNode>(
                ast::AstNodeKind::StaticFieldIterationStatement, statement->range());
            baseQuery->setTypeSyntax(baseType);
            baseQuery->setText(statement->text());
            StaticFieldIterationSelection baseSelection;
            if (!collectStaticFieldCandidates(unit, baseQuery, currentNamespace, ownerName, scope,
                                              projectsFieldType, activeTypes, baseSelection, diagnostics)) return false;
            selection.fields = std::move(baseSelection.fields);
        }
        const auto callerDerivesFromSpecialization = [&]() -> std::optional<bool>
        {
            const ast::AstNode* caller = scope.lexicalTypeSyntax;
            std::unordered_set<const ast::AstNode*> visited;
            while (caller != nullptr)
            {
                if (caller == record->specializedSyntax) return true;
                if (!visited.insert(caller).second || caller->kind() != ast::AstNodeKind::ClassDeclaration)
                    return std::nullopt;
                const auto context = declaredFieldContexts_.find(caller);
                if (context == declaredFieldContexts_.end() || context->second.unit == nullptr ||
                    context->second.unit->origin() != SyntaxUnitOrigin::Source) return std::nullopt;
                GenericLookupScope baseScope;
                baseScope.importedNamespaces.write() = context->second.imports;
                baseScope.aliases.write() = context->second.aliases;
                const ast::AstNode* next = nullptr;
                for (const ast::TypeSyntax* base : caller->typeSyntaxes())
                {
                    if (base == nullptr || !base->modifiers().empty()) return std::nullopt;
                    const ast::AstNode* candidate = nullptr;
                    const std::string* identity = lookupResolvedTypeIdentity(base);
                    if (identity != nullptr && identity->starts_with("generic:"))
                    {
                        const auto baseOrdinal = specializationPlanner_.request(identity->substr(8));
                        if (!baseOrdinal.has_value() || *baseOrdinal >= specializations_.size() ||
                            specializations_[*baseOrdinal].state != SpecializationState::Ready ||
                            specializations_[*baseOrdinal].specializationKey != identity->substr(8))
                            return std::nullopt;
                        candidate = specializations_[*baseOrdinal].specializedSyntax;
                    }
                    else
                    {
                        const DeclaredTypeIdentity* resolved = resolveDeclaredTypeIdentity(base,
                            context->second.unit->projectOrdinal(), context->second.namespaceName, baseScope);
                        if (resolved == nullptr) return std::nullopt;
                        candidate = resolved->syntax;
                    }
                    if (candidate == nullptr) return std::nullopt;
                    if (candidate->kind() == ast::AstNodeKind::InterfaceDeclaration) continue;
                    if (candidate->kind() != ast::AstNodeKind::ClassDeclaration || next != nullptr)
                        return std::nullopt;
                    next = candidate;
                }
                caller = next;
            }
            return false;
        };
        std::vector<const ast::AstNode*> originalFields;
        for (const ast::AstNode* child : record->templateSyntax->children())
            if (child != nullptr && child->kind() == ast::AstNodeKind::FieldDeclaration)
                originalFields.push_back(child);
        std::size_t fieldIndex = 0;
        for (const ast::AstNode* field : record->specializedSyntax->children())
        {
            if (field == nullptr || field->kind() != ast::AstNodeKind::FieldDeclaration) continue;
            const ast::AstNode* source = field;
            std::unordered_set<const ast::AstNode*> seen;
            bool cycle = false;
            bool linked = false;
            while (source != nullptr)
            {
                if (!seen.insert(source).second) { cycle = true; break; }
                const ast::AstNode* parent = fieldCloneSource(source);
                if (parent == nullptr) break;
                linked = true;
                source = parent;
            }
            if (cycle || !linked || fieldIndex >= originalFields.size() || source != originalFields[fieldIndex++] ||
                field->text() != source->text() || field->tokenKind() != source->tokenKind() ||
                field->hasModifier(lexer::TokenKind::KwStatic) != source->hasModifier(lexer::TokenKind::KwStatic) ||
                field->hasModifier(lexer::TokenKind::KwConst) != source->hasModifier(lexer::TokenKind::KwConst))
            {
                diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error, statement->range(),
                    "static field iteration cannot verify cloned field provenance");
                return false;
            }
            if (field->hasModifier(lexer::TokenKind::KwStatic) || field->hasModifier(lexer::TokenKind::KwConst))
                continue;
            bool protectedVisible = false;
            if (field->tokenKind() == lexer::TokenKind::KwProtected)
            {
                const std::optional<bool> derived = callerDerivesFromSpecialization();
                if (!derived.has_value())
                {
                    diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error, statement->range(),
                        "static field iteration cannot yet verify protected access to specialized fields");
                    return false;
                }
                protectedVisible = *derived;
            }
            const bool visible = field->tokenKind() == lexer::TokenKind::KwPublic ||
                (field->tokenKind() == lexer::TokenKind::KwInternal &&
                 state->unit->projectOrdinal() == unit.projectOrdinal()) ||
                scope.lexicalTypeSyntax == record->specializedSyntax || protectedVisible;
            if (!visible) continue;
            const ast::TypeSyntax* fieldType = field->typeSyntax();
            if (!matchingStaticFieldType(fieldType, fieldType, {}) || !field->auxiliaryText().empty())
            {
                diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error, field->range(),
                    "static field iteration cannot yet verify this specialized field type");
                return false;
            }
            selection.fields.push_back({selection.queriedTypeIdentity, field, {}});
        }
        if (fieldIndex != originalFields.size())
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error, statement->range(),
                "static field iteration cannot verify complete cloned fields");
            return false;
        }
        return true;
    }
    const auto nestedVisible = [&](const DeclaredTypeIdentity* declaration)
    {
        if (declaration == nullptr) return false;
        const std::size_t separator = declaration->identityKey.rfind('+');
        if (separator == std::string::npos) return true;
        const std::string container = declaration->identityKey.substr(0, separator);
        return declaration->projectOrdinal == unit.projectOrdinal() &&
            (callerIdentity == container || callerIdentity.starts_with(container + "+"));
    };
    const auto origin = type != nullptr ? declaredFieldContexts_.find(type->syntax)
                                        : declaredFieldContexts_.end();
    const PrecompiledFieldCertificate* binaryCertificate = nullptr;
    if (type != nullptr && origin != declaredFieldContexts_.end() && origin->second.unit != nullptr &&
        origin->second.unit->origin() == SyntaxUnitOrigin::PrecompiledMetadata &&
        type->projectOrdinal != unit.projectOrdinal())
    {
        const auto certificate = precompiledFieldCertificates_.find(type->identityKey);
        bool callerEligible = ownerName.empty() && scope.lexicalTypeSyntax == nullptr;
        if (scope.lexicalTypeSyntax != nullptr)
        {
            const ast::AstNode* caller = scope.lexicalTypeSyntax;
            std::unordered_set<const ast::AstNode*> visitedCallers;
            callerEligible = true;
            while (caller != nullptr)
            {
                if (caller == type->syntax)
                {
                    // A derived caller may inspect only a chain where every
                    // ordinary field has an explicit metadata access record.
                    const PrecompiledFieldCertificate* current =
                        certificate != precompiledFieldCertificates_.end() ? &certificate->second : nullptr;
                    std::unordered_set<std::string> seen;
                    while (current != nullptr)
                    {
                        if (seen.size() >= 1024 || !seen.insert(current->typeIdentity).second ||
                            std::any_of(current->fields.begin(), current->fields.end(), [](const auto& field) {
                                return field.flags != 1024U && !field.accessKnown;
                            })) { callerEligible = false; break; }
                        if (current->baseTypeIdentity.empty()) break;
                        const auto parent = precompiledFieldCertificates_.find(current->baseTypeIdentity);
                        if (parent == precompiledFieldCertificates_.end()) { callerEligible = false; break; }
                        current = &parent->second;
                    }
                    break;
                }
                const auto context = declaredFieldContexts_.find(caller);
                const PrecompiledFieldCertificate* callerCertificate = nullptr;
                if (context != declaredFieldContexts_.end() && context->second.unit != nullptr &&
                    context->second.unit->origin() == SyntaxUnitOrigin::PrecompiledMetadata)
                {
                    const DeclaredTypeIdentity* callerIdentityRecord = nullptr;
                    for (const DeclaredTypeIdentity& candidate : declaredTypeIdentities_)
                    {
                        if (candidate.syntax != caller) continue;
                        if (callerIdentityRecord != nullptr) { callerIdentityRecord = nullptr; break; }
                        callerIdentityRecord = &candidate;
                    }
                    if (callerIdentityRecord != nullptr)
                    {
                        const auto candidate = precompiledFieldCertificates_.find(callerIdentityRecord->identityKey);
                        if (candidate != precompiledFieldCertificates_.end() && candidate->second.isClass)
                            callerCertificate = &candidate->second;
                    }
                }
                if (visitedCallers.size() >= 1024 ||
                    !visitedCallers.insert(caller).second || caller->flag() ||
                    context == declaredFieldContexts_.end() || context->second.unit == nullptr ||
                    (context->second.unit->origin() != SyntaxUnitOrigin::Source && callerCertificate == nullptr) ||
                    (caller->kind() != ast::AstNodeKind::ClassDeclaration &&
                     caller->kind() != ast::AstNodeKind::StructDeclaration))
                { callerEligible = false; break; }
                GenericLookupScope callerScope;
                callerScope.importedNamespaces.write() = context->second.imports;
                callerScope.aliases.write() = context->second.aliases;
                const ast::AstNode* next = nullptr;
                std::string nextIdentity;
                for (const ast::TypeSyntax* base : caller->typeSyntaxes())
                {
                    const std::string* identity = base != nullptr ? lookupResolvedTypeIdentity(base) : nullptr;
                    if (base == nullptr || !base->typeArguments().empty() || !base->modifiers().empty() ||
                        (identity != nullptr && identity->starts_with("generic:")))
                    { callerEligible = false; break; }
                    const DeclaredTypeIdentity* resolved = resolveDeclaredTypeIdentity(base,
                        context->second.unit->projectOrdinal(), context->second.namespaceName, callerScope);
                    if (resolved == nullptr || resolved->syntax == nullptr)
                    { callerEligible = false; break; }
                    if (resolved->syntax->kind() == ast::AstNodeKind::InterfaceDeclaration) continue;
                    if (caller->kind() == ast::AstNodeKind::StructDeclaration || next != nullptr ||
                        resolved->syntax->kind() != ast::AstNodeKind::ClassDeclaration)
                    { callerEligible = false; break; }
                    next = resolved->syntax;
                    nextIdentity = resolved->identityKey;
                }
                if (callerCertificate != nullptr && callerCertificate->baseTypeIdentity != nextIdentity)
                    callerEligible = false;
                if (!callerEligible) break;
                caller = next;
            }
        }
        if (certificate != precompiledFieldCertificates_.end() &&
            (!certificate->second.isClass || callerEligible))
            binaryCertificate = &certificate->second;
    }
    if (type == nullptr || type->syntax == nullptr || queried == nullptr ||
        !queried->modifiers().empty() ||
        (type->syntax->kind() != ast::AstNodeKind::StructDeclaration &&
         type->syntax->kind() != ast::AstNodeKind::ClassDeclaration) ||
        type->syntax->flag() || !type->syntax->genericParameters().empty() ||
        origin == declaredFieldContexts_.end() || origin->second.unit == nullptr ||
        (origin->second.unit->origin() != SyntaxUnitOrigin::Source && binaryCertificate == nullptr) ||
        scope.typeParameters.read().contains(statement->text()) || !nestedVisible(type))
    {
        diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error, statement->range(),
            "static field iteration cannot verify this type's complete field declarations");
        return false;
    }
    selection.queriedTypeIdentity = type->identityKey;
    // Before semantic inheritance exists, protected selection needs a
    // complete, uniquely resolved lexical caller hierarchy. Unknown is not
    // treated as inaccessible: it must fail closed before specialization.
    const auto callerDerivesFrom = [&](const std::string& target) -> std::optional<bool>
    {
        if (ownerName.empty()) return false;
        const DeclaredTypeIdentity* caller = nullptr;
        for (const DeclaredTypeIdentity& candidate : declaredTypeIdentities_)
        {
            if (candidate.identityKey != callerIdentity || candidate.projectOrdinal != unit.projectOrdinal()) continue;
            if (caller != nullptr) return std::nullopt;
            caller = &candidate;
        }
        if (caller == nullptr) return std::nullopt;
        std::unordered_set<const ast::AstNode*> seen;
        while (caller != nullptr)
        {
            if (caller->identityKey == target) return true;
            if (caller->syntax == nullptr || caller->syntax->kind() != ast::AstNodeKind::ClassDeclaration ||
                caller->syntax->flag() || !caller->syntax->genericParameters().empty() ||
                !seen.insert(caller->syntax).second) return std::nullopt;
            const auto context = declaredFieldContexts_.find(caller->syntax);
            if (context == declaredFieldContexts_.end() || context->second.unit == nullptr) return std::nullopt;
            const PrecompiledFieldCertificate* callerCertificate = nullptr;
            if (context->second.unit->origin() == SyntaxUnitOrigin::PrecompiledMetadata)
            {
                const auto found = precompiledFieldCertificates_.find(caller->identityKey);
                if (found != precompiledFieldCertificates_.end() && found->second.isClass)
                    callerCertificate = &found->second;
            }
            if (context->second.unit->origin() != SyntaxUnitOrigin::Source && callerCertificate == nullptr)
                return std::nullopt;
            GenericLookupScope callerScope;
            callerScope.importedNamespaces.write() = context->second.imports;
            callerScope.aliases.write() = context->second.aliases;
            const DeclaredTypeIdentity* next = nullptr;
            for (const ast::TypeSyntax* base : caller->syntax->typeSyntaxes())
            {
                if (base == nullptr || !base->typeArguments().empty() || !base->modifiers().empty()) return std::nullopt;
                const DeclaredTypeIdentity* resolved = resolveDeclaredTypeIdentity(base,
                    context->second.unit->projectOrdinal(), context->second.namespaceName, callerScope);
                if (resolved == nullptr || resolved->syntax == nullptr) return std::nullopt;
                if (resolved->syntax->kind() == ast::AstNodeKind::InterfaceDeclaration) continue;
                if (resolved->syntax->kind() != ast::AstNodeKind::ClassDeclaration || next != nullptr)
                    return std::nullopt;
                next = resolved;
            }
            if (callerCertificate != nullptr && callerCertificate->baseTypeIdentity !=
                (next != nullptr ? next->identityKey : std::string{})) return std::nullopt;
            caller = next;
        }
        return false;
    };
    std::vector<const DeclaredTypeIdentity*> hierarchy;
    std::vector<const PrecompiledFieldCertificate*> hierarchyCertificates;
    ast::TypeSyntax* genericSourceBase = nullptr;
    std::unordered_set<const ast::AstNode*> visited;
    for (const DeclaredTypeIdentity* current = type; current != nullptr;)
    {
        const auto context = declaredFieldContexts_.find(current->syntax);
        const PrecompiledFieldCertificate* currentCertificate = nullptr;
        if (binaryCertificate != nullptr)
        {
            const auto found = precompiledFieldCertificates_.find(current->identityKey);
            if (found != precompiledFieldCertificates_.end()) currentCertificate = &found->second;
        }
        if (current->syntax == nullptr || !visited.insert(current->syntax).second ||
            current->syntax->flag() || !current->syntax->genericParameters().empty() ||
            context == declaredFieldContexts_.end() || context->second.unit == nullptr ||
            (context->second.unit->origin() != SyntaxUnitOrigin::Source && currentCertificate == nullptr) ||
            (binaryCertificate != nullptr &&
             (context->second.unit->origin() != SyntaxUnitOrigin::PrecompiledMetadata || currentCertificate == nullptr)))
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error, statement->range(),
                "static field iteration cannot verify the complete class hierarchy");
            return false;
        }
        hierarchy.push_back(current);
        hierarchyCertificates.push_back(currentCertificate);
        const DeclaredTypeIdentity* baseClass = nullptr;
        GenericLookupScope baseScope;
        baseScope.importedNamespaces.write() = context->second.imports;
        baseScope.aliases.write() = context->second.aliases;
        for (ast::TypeSyntax* base : current->syntax->typeSyntaxes())
        {
            const std::string* genericBaseIdentity = base != nullptr ? lookupResolvedTypeIdentity(base) : nullptr;
            if (genericBaseIdentity != nullptr && genericBaseIdentity->starts_with("generic:") &&
                current == type && currentCertificate == nullptr &&
                context->second.unit->origin() == SyntaxUnitOrigin::Source &&
                current->syntax->typeSyntaxes().size() == 1)
            {
                const auto ordinal = specializationPlanner_.request(genericBaseIdentity->substr(8));
                const GenericSpecializationRecord* record = ordinal.has_value() && *ordinal < specializations_.size()
                    ? &specializations_[*ordinal] : nullptr;
                const TemplateState* baseState = record != nullptr && record->templateOrdinal < templateStates_.size()
                    ? &templateStates_[record->templateOrdinal] : nullptr;
                if (record == nullptr || record->state != SpecializationState::Ready ||
                    record->kind != GenericTemplateKind::Class || record->specializedSyntax == nullptr ||
                    baseState == nullptr || baseState->unit == nullptr ||
                    baseState->unit->origin() != SyntaxUnitOrigin::Source ||
                    record->templateSyntax != baseState->syntax ||
                    record->specializationKey != genericBaseIdentity->substr(8) ||
                    !templateVisible(context->second.unit->projectOrdinal(), baseState->record))
                {
                    diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error, statement->range(),
                        "static field iteration cannot verify the complete class hierarchy");
                    return false;
                }
                genericSourceBase = base;
                continue;
            }
            const DeclaredTypeIdentity* resolved = base != nullptr && base->typeArguments().empty() &&
                    base->modifiers().empty()
                ? resolveDeclaredTypeIdentity(base, context->second.unit->projectOrdinal(),
                                              context->second.namespaceName, baseScope)
                : nullptr;
            if (resolved == nullptr && currentCertificate != nullptr && base != nullptr &&
                base->typeArguments().empty() && base->modifiers().empty())
            {
                const auto alias = baseScope.aliases.read().find(base->baseName());
                if (alias != baseScope.aliases.read().end() && alias->second.find("::") != std::string::npos)
                    resolved = resolveModuleQualifiedDeclaredTypeIdentity(
                        alias->second, context->second.unit->projectOrdinal());
            }
            if (resolved == nullptr || resolved->syntax == nullptr ||
                (resolved->syntax->kind() != ast::AstNodeKind::ClassDeclaration &&
                 resolved->syntax->kind() != ast::AstNodeKind::InterfaceDeclaration) ||
                (resolved->syntax->kind() == ast::AstNodeKind::ClassDeclaration && baseClass != nullptr))
            {
                diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error, statement->range(),
                    "static field iteration cannot verify the complete class hierarchy");
                return false;
            }
            if (resolved->syntax->kind() == ast::AstNodeKind::ClassDeclaration) baseClass = resolved;
        }
        if (currentCertificate != nullptr &&
            currentCertificate->baseTypeIdentity != (baseClass != nullptr ? baseClass->identityKey : std::string{}))
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error, statement->range(),
                "static field iteration cannot verify the binary base identity");
            return false;
        }
        current = baseClass;
    }
    if (genericSourceBase != nullptr)
    {
        ast::AstNode* baseQuery = unit.arena().make<ast::AstNode>(
            ast::AstNodeKind::StaticFieldIterationStatement, statement->range());
        baseQuery->setTypeSyntax(genericSourceBase);
        baseQuery->setText(statement->text());
        StaticFieldIterationSelection baseSelection;
        if (!collectStaticFieldCandidates(unit, baseQuery, currentNamespace, ownerName, scope,
                                          projectsFieldType, activeTypes, baseSelection, diagnostics)) return false;
        selection.fields = std::move(baseSelection.fields);
    }
    for (std::size_t hierarchyIndex = 0; hierarchyIndex < hierarchy.size(); ++hierarchyIndex)
    {
        const PrecompiledFieldCertificate* certificate = hierarchyCertificates[hierarchyIndex];
        if (certificate == nullptr) continue;
        const DeclaredTypeIdentity* owner = hierarchy[hierarchyIndex];
        if ((owner->syntax->kind() == ast::AstNodeKind::ClassDeclaration) != certificate->isClass)
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error, statement->range(),
                "static field iteration cannot verify reconstructed binary type kind");
            return false;
        }
        std::size_t index = 0;
        bool complete = true;
        std::uint64_t classOffset = certificate->baseTypeIdentity.empty() ? 8 : certificate->baseSize;
        std::uint64_t classAlignment = certificate->baseTypeIdentity.empty() ? 8 : certificate->baseAlignment;
        if (!certificate->baseTypeIdentity.empty())
        {
            const auto base = precompiledFieldCertificates_.find(certificate->baseTypeIdentity);
            if (base == precompiledFieldCertificates_.end() || !base->second.isClass ||
                base->second.classSize != certificate->baseSize ||
                base->second.classAlignment != certificate->baseAlignment ||
                certificate->baseSize < 8 || certificate->baseAlignment < 8 ||
                (certificate->baseAlignment & (certificate->baseAlignment - 1U)) != 0)
                complete = false;
        }
        for (const ast::AstNode* field : owner->syntax->children())
        {
            if (field == nullptr || field->kind() != ast::AstNodeKind::FieldDeclaration ||
                field->hasModifier(lexer::TokenKind::KwStatic) || field->hasModifier(lexer::TokenKind::KwConst))
                continue;
            if (index >= certificate->fields.size()) { complete = false; break; }
            const PrecompiledFieldCertificateEntry& expected = certificate->fields[index];
            if (certificate->isClass)
            {
                const std::uint64_t naturalSize = expected.typeKey == "builtin:int" ? 4U :
                    expected.typeKey == "builtin:long" ? 8U : 0U;
                const std::uint64_t alignment = expected.alignment;
                if (naturalSize == 0 || expected.size != naturalSize || alignment < naturalSize ||
                    (alignment & (alignment - 1U)) != 0 ||
                    classOffset > std::numeric_limits<std::uint64_t>::max() - (alignment - 1U))
                { complete = false; break; }
                classOffset = (classOffset + alignment - 1U) & ~(alignment - 1U);
                if (classOffset != expected.offset ||
                    classOffset > std::numeric_limits<std::uint64_t>::max() - expected.size)
                { complete = false; break; }
                classOffset += expected.size;
                classAlignment = std::max(classAlignment, alignment);
            }
            const bool generated = expected.flags == 1024U;
            const std::string expectedName = generated
                ? "__nsc_layout_property_" + std::to_string(index) : expected.name;
            const ast::TypeSyntax* syntax = field->typeSyntax();
            bool matchingType = syntax != nullptr && lexer::isBuiltinTypeKeyword(syntax->baseTokenKind()) &&
                "builtin:" + syntax->baseName() == expected.typeKey;
            if (syntax != nullptr && syntax->baseTokenKind() == lexer::TokenKind::Identifier &&
                syntax->typeArguments().empty())
            {
                const auto fieldContext = declaredFieldContexts_.find(field);
                if (fieldContext != declaredFieldContexts_.end() && fieldContext->second.unit != nullptr)
                {
                    GenericLookupScope fieldScope;
                    fieldScope.importedNamespaces.write() = fieldContext->second.imports;
                    fieldScope.aliases.write() = fieldContext->second.aliases;
                    const DeclaredTypeIdentity* resolved = resolveDeclaredTypeIdentity(syntax,
                        fieldContext->second.unit->projectOrdinal(), fieldContext->second.namespaceName, fieldScope);
                    matchingType = resolved != nullptr && "type:" + resolved->identityKey == expected.typeKey;
                }
            }
            if ((expected.flags != 0U && !generated) ||
                (generated && (!expected.name.starts_with("__nsc_backing_") ||
                               !expected.name.ends_with("$stage38") || expected.isPublic)) ||
                field->text() != expectedName || field->typeSyntax() == nullptr || !matchingType ||
                field->hasModifier(lexer::TokenKind::KwReadonly) ||
                field->hasModifier(lexer::TokenKind::KwVolatile) ||
                field->typeSyntax()->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None ||
                field->typeSyntax()->referenceQualifier() != ast::ReferenceQualifierSyntaxKind::None ||
                field->auxiliaryText() != (certificate->isClass ? "" :
                    "at:" + std::to_string(expected.offset) + ";") +
                    "field-align:" + std::to_string(expected.alignment) ||
                (field->tokenKind() == lexer::TokenKind::KwPublic) != expected.isPublic ||
                (field->tokenKind() == lexer::TokenKind::KwProtected) != expected.isProtected ||
                (!expected.accessKnown && field->tokenKind() != lexer::TokenKind::KwPrivate))
            { complete = false; break; }
            ++index;
        }
        if (certificate->isClass && complete &&
            (classOffset > std::numeric_limits<std::uint64_t>::max() - (classAlignment - 1U) ||
             ((classOffset + classAlignment - 1U) & ~(classAlignment - 1U)) != certificate->classSize ||
             classAlignment != certificate->classAlignment)) complete = false;
        if (!complete || index != certificate->fields.size())
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error, statement->range(),
                "static field iteration cannot verify reconstructed binary fields");
            return false;
        }
    }
    for (auto part = hierarchy.rbegin(); part != hierarchy.rend(); ++part)
    for (const ast::AstNode* field : (*part)->syntax->children())
    {
        if (field == nullptr || field->kind() != ast::AstNodeKind::FieldDeclaration ||
            field->hasModifier(lexer::TokenKind::KwStatic) || field->hasModifier(lexer::TokenKind::KwConst))
            continue;
        bool protectedVisible = false;
        if (field->tokenKind() == lexer::TokenKind::KwProtected)
        {
            const std::optional<bool> derived = callerDerivesFrom((*part)->identityKey);
            if (!derived.has_value())
            {
                diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error, field->range(),
                    "static field iteration cannot verify protected access in this context");
                return false;
            }
            protectedVisible = *derived;
        }
        const bool visible = field->tokenKind() == lexer::TokenKind::KwPublic ||
            (field->tokenKind() == lexer::TokenKind::KwInternal &&
             (*part)->projectOrdinal == unit.projectOrdinal()) || protectedVisible ||
            callerIdentity == (*part)->identityKey;
        if (!visible) continue;
        const ast::TypeSyntax* fieldType = field->typeSyntax();
        std::string projectedTypeIdentity;
        const ast::TypeSyntax* projectedTypeSyntax = nullptr;
        bool verifiedType = fieldType != nullptr && lexer::isBuiltinTypeKeyword(fieldType->baseTokenKind()) &&
            fieldType->baseTokenKind() != lexer::TokenKind::KwVoid;
        if (binaryCertificate == nullptr && verifiedSourceBuiltinTuple(fieldType)) verifiedType = true;
        if (!verifiedType && binaryCertificate == nullptr && fieldType != nullptr && fieldType->isTuple())
        {
            const auto context = declaredFieldContexts_.find(field);
            if (context != declaredFieldContexts_.end() && context->second.unit != nullptr &&
                context->second.unit->origin() == SyntaxUnitOrigin::Source)
            {
                GenericLookupScope fieldScope;
                fieldScope.importedNamespaces.write() = context->second.imports;
                fieldScope.aliases.write() = context->second.aliases;
                std::string fieldOwner = (*part)->sourceName;
                if (!context->second.namespaceName.empty() &&
                    fieldOwner.starts_with(context->second.namespaceName + "."))
                    fieldOwner.erase(0, context->second.namespaceName.size() + 1);
                std::replace(fieldOwner.begin(), fieldOwner.end(), '.', '+');
                std::vector<const ast::TypeSyntax*> work{fieldType};
                std::vector<std::string> nominalNames;
                std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
                bool complete = true;
                std::size_t visits = 0;
                while (!work.empty() && complete)
                {
                    const ast::TypeSyntax* part = work.back();
                    work.pop_back();
                    if (part == nullptr || ++visits > 100000 || !part->modifiers().empty() ||
                        part->isExplicitTraitImplementation()) { complete = false; break; }
                    if (part->isTuple())
                    {
                        for (const ast::TypeSyntax* child : part->typeArguments()) work.push_back(child);
                    }
                    else if (part->baseTokenKind() == lexer::TokenKind::Identifier && part->typeArguments().empty())
                    {
                        nestedAmbiguous = false;
                        const DeclaredTypeIdentity* declared = resolveNested(part, fieldOwner,
                            context->second.namespaceName, context->second.unit->projectOrdinal());
                        if (declared == nullptr && !nestedAmbiguous)
                            declared = resolveDeclaredTypeIdentity(part, context->second.unit->projectOrdinal(),
                                                                   context->second.namespaceName, fieldScope);
                        std::string qualifiedType = declared != nullptr ? declared->identityKey : std::string{};
                        std::replace(qualifiedType.begin(), qualifiedType.end(), '+', '.');
                        const DeclaredTypeIdentity* projected = declared != nullptr
                            ? resolveModuleQualifiedDeclaredTypeIdentity(qualifiedType, unit.projectOrdinal()) : nullptr;
                        if (declared == nullptr || projected == nullptr || declared->syntax == nullptr ||
                            declared->syntax != projected->syntax || declared->identityKey != projected->identityKey ||
                            declared->projectOrdinal != projected->projectOrdinal ||
                            !nestedVisible(declared) ||
                            (declared->projectOrdinal != unit.projectOrdinal() &&
                             (!declared->isPublic || !projectVisible(unit.projectOrdinal(), declared->projectOrdinal))))
                        { complete = false; break; }
                        nominalNames.push_back(part->baseName());
                        substitutions.emplace(part->baseName(), unit.arena().make<ast::TypeSyntax>(
                            part->range(), lexer::TokenKind::Identifier, std::move(qualifiedType)));
                    }
                }
                if (complete && !nominalNames.empty() && matchingStaticFieldType(fieldType, fieldType, nominalNames))
                {
                    projectedTypeSyntax = cloneType(unit.arena(), fieldType, substitutions);
                    verifiedType = projectedTypeSyntax != nullptr;
                }
            }
        }
        if (fieldType != nullptr && fieldType->baseTokenKind() == lexer::TokenKind::Identifier &&
            fieldType->typeArguments().empty())
        {
            const auto context = declaredFieldContexts_.find(field);
            if (context != declaredFieldContexts_.end() && context->second.unit != nullptr &&
                (context->second.unit->origin() == SyntaxUnitOrigin::Source || binaryCertificate != nullptr))
            {
                GenericLookupScope fieldScope;
                fieldScope.importedNamespaces.write() = context->second.imports;
                fieldScope.aliases.write() = context->second.aliases;
                std::string fieldOwner = (*part)->sourceName;
                if (!context->second.namespaceName.empty() &&
                    fieldOwner.starts_with(context->second.namespaceName + "."))
                    fieldOwner.erase(0, context->second.namespaceName.size() + 1);
                std::replace(fieldOwner.begin(), fieldOwner.end(), '.', '+');
                nestedAmbiguous = false;
                const DeclaredTypeIdentity* declared = resolveNested(fieldType, fieldOwner,
                    context->second.namespaceName, context->second.unit->projectOrdinal());
                if (declared == nullptr && !nestedAmbiguous)
                    declared = resolveDeclaredTypeIdentity(fieldType,
                        context->second.unit->projectOrdinal(), context->second.namespaceName, fieldScope);
                std::string qualifiedType = declared != nullptr ? declared->identityKey : std::string{};
                std::replace(qualifiedType.begin(), qualifiedType.end(), '+', '.');
                const DeclaredTypeIdentity* projected = declared != nullptr
                    ? resolveModuleQualifiedDeclaredTypeIdentity(qualifiedType, unit.projectOrdinal()) : nullptr;
                verifiedType = declared != nullptr && projected != nullptr && declared->syntax != nullptr &&
                    declared->syntax == projected->syntax && declared->projectOrdinal == projected->projectOrdinal &&
                    declared->identityKey == projected->identityKey &&
                    nestedVisible(declared) &&
                    (declared->projectOrdinal == unit.projectOrdinal() ||
                     (declared->isPublic && projectVisible(unit.projectOrdinal(), declared->projectOrdinal)));
                if (verifiedType) projectedTypeIdentity = std::move(qualifiedType);
                else if (binaryCertificate != nullptr && projectsFieldType && declared != nullptr &&
                         declared->syntax != nullptr && !declared->isPublic &&
                         declared->projectOrdinal != unit.projectOrdinal() &&
                         projectVisible(unit.projectOrdinal(), declared->projectOrdinal))
                {
                    diagnostics.report(diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error, statement->range(),
                        "static field type is not accessible in this context");
                    return false;
                }
                else if (binaryCertificate != nullptr && !projectsFieldType && declared != nullptr &&
                         declared->syntax != nullptr && !declared->isPublic &&
                         declared->projectOrdinal != unit.projectOrdinal() &&
                         projectVisible(unit.projectOrdinal(), declared->projectOrdinal))
                    verifiedType = true;
            }
        }
        const bool verifiedArray = binaryCertificate == nullptr && verifiedSourceConstantIntArray(fieldType);
        if (!verifiedType || (!fieldType->modifiers().empty() && !verifiedArray) ||
            (binaryCertificate == nullptr && !field->auxiliaryText().empty()))
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error, field->range(),
                "static field iteration cannot yet verify this field's type or representation");
            return false;
        }
        selection.fields.push_back({(*part)->identityKey, field, std::move(projectedTypeIdentity), projectedTypeSyntax});
    }
    return true;
}

// Constructs detached copies first. No iteration or access identity is
// published unless every candidate body can be rewritten successfully.
bool ProjectGenericModel::expandStaticFieldIteration(
    ProjectSyntaxUnit& unit,
    ast::AstNode* statement,
    const std::string& currentNamespace,
    const std::string& ownerName,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::string typeError;
    std::size_t bodyNodes = 0;
    bool projectsFieldType = false;
    if (!internal::validateStaticFieldTemplateTypes(
            statement->first(), statement->text(), bodyNodes, projectsFieldType, typeError))
    {
        diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error, statement->range(), std::move(typeError));
        return false;
    }
    StaticFieldIterationSelection selection;
    std::unordered_set<const ast::AstNode*> activeTypes;
    if (!collectStaticFieldCandidates(unit, statement, currentNamespace, ownerName, scope,
                                      projectsFieldType, activeTypes, selection, diagnostics))
        return false;
    // Validate the open body even when no fields are selected. Rewriting only
    // per-field copies would otherwise silently accept unsupported constructs
    // whenever the resulting iteration has zero copies.
    std::vector<ast::AstNode*> templateNodes;
    std::vector<internal::StaticFieldRewriteEdge> templateEdges;
    std::unordered_map<ast::AstNode*, std::vector<std::size_t>> incoming;
    std::vector<ast::AstNode*> names;
    std::vector<ast::AstNode*> gets;
    std::string rewriteError;
    if (!internal::collectStaticFieldRewriteGraph(statement->first(), templateNodes, templateEdges, incoming, rewriteError) ||
        !internal::staticFieldRewriteGraphIsAcyclic(templateNodes, templateEdges, rewriteError))
    {
        diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error, statement->range(), std::move(rewriteError));
        return false;
    }
    bodyNodes = templateNodes.size();
    std::vector<ast::AstNode*> scopedNodes;
    std::vector<internal::StaticFieldRewriteEdge> scopedEdges;
    std::unordered_map<ast::AstNode*, std::vector<std::size_t>> scopedIncoming;
    if (!internal::collectStaticFieldRewriteGraph(statement->first(), scopedNodes, scopedEdges,
                                                   scopedIncoming, rewriteError, statement->text()) ||
        !internal::validateStaticFieldRewrite(scopedNodes, scopedEdges, scopedIncoming,
                                               statement->text(), names, gets, rewriteError,
                                               scope.activeLoopLabels.read(), !selection.fields.empty()))
    {
        diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error, statement->range(), std::move(rewriteError));
        return false;
    }
    const std::unordered_set<ast::AstNode*> outerScopeNodes(scopedNodes.begin(), scopedNodes.end());
    for (ast::AstNode* nested : scopedNodes)
    {
        if (nested->kind() != ast::AstNodeKind::StaticFieldIterationStatement ||
            nested->text() != statement->text() || nested->first() == nullptr) continue;
        std::vector<ast::AstNode*> hiddenNodes;
        std::vector<internal::StaticFieldRewriteEdge> hiddenEdges;
        std::unordered_map<ast::AstNode*, std::vector<std::size_t>> hiddenIncoming;
        if (!internal::collectStaticFieldRewriteGraph(nested->first(), hiddenNodes, hiddenEdges,
                                                       hiddenIncoming, rewriteError))
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error, nested->range(), std::move(rewriteError));
            return false;
        }
        for (ast::AstNode* hidden : hiddenNodes)
        {
            if (!outerScopeNodes.contains(hidden)) continue;
            diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error, nested->range(),
                "static field descriptor scopes share syntax nodes");
            return false;
        }
    }
    // Check nested open bodies even when this selection is empty: otherwise an
    // invalid inner descriptor or control-flow target could disappear silently.
    std::size_t nestedValidationNodes = 0;
    for (ast::AstNode* nested : templateNodes)
    {
        if (nested->kind() != ast::AstNodeKind::StaticFieldIterationStatement) continue;
        std::size_t nestedCount = 0;
        bool nestedProjectsType = false;
        std::vector<ast::AstNode*> nodes;
        std::vector<internal::StaticFieldRewriteEdge> edges;
        std::unordered_map<ast::AstNode*, std::vector<std::size_t>> nestedIncoming;
        std::vector<ast::AstNode*> nestedNames;
        std::vector<ast::AstNode*> nestedGets;
        if (nested->first() == nullptr ||
            !internal::validateStaticFieldTemplateTypes(nested->first(), nested->text(), nestedCount,
                                                        nestedProjectsType, rewriteError) ||
            nestedCount > 100000 - nestedValidationNodes ||
            !internal::collectStaticFieldRewriteGraph(nested->first(), nodes, edges, nestedIncoming,
                                                      rewriteError, nested->text()) ||
            !internal::staticFieldRewriteGraphIsAcyclic(nodes, edges, rewriteError) ||
            !internal::validateStaticFieldRewrite(nodes, edges, nestedIncoming, nested->text(),
                                                  nestedNames, nestedGets, rewriteError,
                                                  scope.activeLoopLabels.read(), !selection.fields.empty()))
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error, nested->range(), rewriteError.empty()
                    ? "static field nested body exceeds validation budget or is invalid" : std::move(rewriteError));
            return false;
        }
        nestedValidationNodes += nestedCount;
    }
    constexpr std::size_t maximumExpandedNodes = 100000;
    if (bodyNodes != 0 && selection.fields.size() > maximumExpandedNodes / bodyNodes)
    {
        diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error, statement->range(),
            "static field iteration exceeds its syntax expansion budget");
        return false;
    }
    std::vector<ast::AstNode*> jumps;
    if (!internal::collectStaticFieldJumps(statement->first(), jumps, rewriteError))
    {
        diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error, statement->range(), std::move(rewriteError));
        return false;
    }
    ast::AstArena& arena = unit.arena();
    ast::AstNode* block = arena.make<ast::AstNode>(ast::AstNodeKind::BlockStatement, statement->range());
    ast::AstNode* loopBody = jumps.empty() ? block : arena.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, statement->range());
    const std::string suffix = std::to_string(statement->range().begin().fileId().value()) + "$" +
        std::to_string(statement->range().begin().offset());
    const std::string outerLabel = "__nsc$static_field_outer$" + suffix;
    std::vector<std::pair<const ast::AstNode*, StaticFieldAccessSelection>> pending;
    std::size_t iteration = 0;
    for (const StaticFieldAccessSelection& field : selection.fields)
    {
        const ast::TypeSyntax* projectedType = field.projectedTypeSyntax != nullptr
            ? field.projectedTypeSyntax : field.fieldDeclaration->typeSyntax();
        if (!field.projectedTypeIdentity.empty())
            projectedType = arena.make<ast::TypeSyntax>(projectedType->range(),
                lexer::TokenKind::Identifier, field.projectedTypeIdentity);
        const std::unordered_map<std::string, const ast::TypeSyntax*> substitutions{
            {statement->text() + ".Type", projectedType}};
        ast::AstNode* body = cloneNode(arena, statement->first(), substitutions, {}, {});
        std::vector<const ast::AstNode*> accesses;
        std::string error;
        if (!internal::rewriteStaticFieldTemplate(
                arena, body, statement->text(), field.fieldDeclaration->text(), accesses, error,
                scope.activeLoopLabels.read()))
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error, statement->range(), std::move(error));
            return false;
        }
        for (const ast::AstNode* access : accesses) pending.emplace_back(access, field);
        if (jumps.empty()) block->addChild(body);
        else
        {
            const std::string innerLabel = "__nsc$static_field_inner$" + suffix + "$" + std::to_string(iteration++);
            if (!internal::rewriteStaticFieldJumps(arena, body, outerLabel, innerLabel, error))
            {
                diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error, statement->range(), std::move(error));
                return false;
            }
            loopBody->addChild(internal::staticFieldLoopLocal(arena, statement->range(),
                innerLabel + "$result", innerLabel, body));
        }
    }
    if (!jumps.empty()) block->addChild(internal::staticFieldLoopLocal(arena, statement->range(),
        outerLabel + "$result", outerLabel, loopBody));
    statement->setFirst(block);
    staticFieldIterationSelections_.emplace(statement, std::move(selection));
    for (auto& [access, field] : pending) staticFieldAccessSelections_.emplace(access, std::move(field));
    return true;
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
    if (node->kind() == ast::AstNodeKind::StaticFieldIterationStatement)
    {
        ast::TypeSyntax* queried = node->typeSyntax();
        if (queried != nullptr && queried->typeArguments().empty() &&
            !deferredStaticFieldNodes_.contains(node))
        {
            const DeclaredTypeIdentity* declared = resolveDeclaredTypeIdentity(
                queried, unit.projectOrdinal(), currentNamespace, currentScope);
            const auto context = declared != nullptr && declared->syntax != nullptr
                ? declaredFieldContexts_.find(declared->syntax) : declaredFieldContexts_.end();
            if (declared != nullptr && declared->syntax != nullptr &&
                declared->projectOrdinal == unit.projectOrdinal() &&
                declared->syntax->kind() == ast::AstNodeKind::ClassDeclaration &&
                declared->syntax->typeSyntaxes().size() == 1 &&
                declared->syntax->typeSyntaxes()[0] != nullptr &&
                !declared->syntax->typeSyntaxes()[0]->typeArguments().empty() &&
                context != declaredFieldContexts_.end() && context->second.unit != nullptr &&
                context->second.unit->origin() == SyntaxUnitOrigin::Source)
            {
                constexpr std::size_t maximumDeferredIterations = 10000;
                if (deferredStaticFieldNodes_.size() >= maximumDeferredIterations)
                {
                    diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error, node->range(),
                        "static field iteration cannot resolve its generic type within the expansion budget");
                    return false;
                }
                deferredStaticFieldNodes_.insert(node);
                deferredStaticFieldIterations_.emplace_back(&unit, node, currentNamespace, ownerName, currentScope);
                return false;
            }
        }
        if (queried != nullptr && !queried->typeArguments().empty() &&
            !hasOpenGenericParameter(queried, currentScope))
        {
            constexpr std::size_t maximumDeferredIterations = 10000;
            if (deferredStaticFieldNodes_.size() >= maximumDeferredIterations ||
                !deferredStaticFieldNodes_.insert(node).second)
            {
                diagnostics.report(diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error, node->range(),
                    "static field iteration cannot resolve its generic type within the expansion budget");
                return false;
            }
            if (!processTypeSyntax(unit, queried, currentNamespace, currentScope, diagnostics)) return false;
            deferredStaticFieldIterations_.emplace_back(&unit, node, currentNamespace, ownerName, currentScope);
            return false;
        }
        if (!expandStaticFieldIteration(
                unit, node, currentNamespace, ownerName, currentScope, diagnostics))
            return false;
    }
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
    GenericLookupScope loopBodyScope = currentScope;
    if (node->kind() == ast::AstNodeKind::LoopExpression && !node->text().empty())
        loopBodyScope.activeLoopLabels.write().push_back(node);
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
            if (declaredLocalType != nullptr)
                substitutedTypeCloneSources_.emplace(declaredLocalType, child->typeSyntax());
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
        childScopes.push_back(node->kind() == ast::AstNodeKind::LoopExpression && child == node->first()
            ? loopBodyScope : sequentialScope);
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
        frames.push_back(ProcessFrame{node->third(), currentNamespace, ownerName,
            node->kind() == ast::AstNodeKind::LoopExpression && node->third() == node->first()
                ? loopBodyScope : currentScope, false});
    if (node->second() != nullptr)
        frames.push_back(ProcessFrame{node->second(), currentNamespace, ownerName,
            node->kind() == ast::AstNodeKind::LoopExpression && node->second() == node->first()
                ? loopBodyScope : currentScope, false});
    if (node->first() != nullptr)
        frames.push_back(ProcessFrame{node->first(), currentNamespace, ownerName,
            node->kind() == ast::AstNodeKind::LoopExpression ? loopBodyScope : currentScope, false});
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
    const std::size_t enclosingGroup = currentOverloadGroup_;
    frames.push_back(
        ProcessFrame{
            node, std::move(namespaceName), std::move(ownerName),
            std::move(scope), insideTemplate, enclosingGroup});
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
        currentOverloadGroup_ = frame.overloadGroup;
        const auto specialization = specializationBySyntax_.find(node);
        if (specialization != specializationBySyntax_.end() &&
            specialization->second < specializations_.size())
            currentOverloadGroup_ = specializations_[specialization->second].overloadGroup;
        const std::size_t nodeGroup = currentOverloadGroup_;

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

    currentOverloadGroup_ = nodeGroup;
    const std::size_t priorFrameCount = frames.size();
    scheduleProcessedNodeChildren(
        unit, node, currentNamespace, ownerName, currentScope, frames);
    for (std::size_t i = priorFrameCount; i < frames.size(); ++i)
        frames[i].overloadGroup = nodeGroup;
    }
    currentOverloadGroup_ = enclosingGroup;
}
}
