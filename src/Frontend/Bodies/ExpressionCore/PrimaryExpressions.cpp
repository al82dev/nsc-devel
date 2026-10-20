#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "../Support/OwnershipArgumentClassification.h"
#include "../Support/OperatorClassification.h"
#include "../Support/IntegralConstantArithmetic.h"
#include "../Support/OwnershipFlowSnapshotQueries.h"


#include "../MethodBodyInternals.h"
#include "../../Internal/Stage37ConstantValue.h"

#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"

#include "nativesharp/Frontend/EnumUnderlyingType.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"
#include "nativesharp/Frontend/ProjectMemberAccessibility.h"
#include "nativesharp/Frontend/ProjectCallableResultMerger.h"
#include "nativesharp/Frontend/ProjectCallableWaveReducer.h"
#include "nativesharp/Frontend/ProjectCallableWaveScheduler.h"
#include "nativesharp/Frontend/ProjectReferenceEscapeEvaluator.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/MemoryModel.h"
#include "nativesharp/Language/SumTypeSemantics.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <ranges>
#include <string>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
using method_body_detail::pointerContract;
using method_body_detail::Stage37ConstantKind;
using method_body_detail::Stage37ConstantValue;
using method_body_detail::writeMethodBodyTrace;

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindLoopExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::LoopExpression:
            result = analyzeLoopExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindMatchExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::MatchExpression:
            result = analyzeMatchExpression(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindMatchValueExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::MatchValueExpression:
            if (activeMatchValues_.empty())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "match governing value is only available inside a match arm");
            }
            else
            {
                result = activeMatchValues_.back();
            }
            break;
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindMatchArmExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::MatchArmExpression:
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "a match arm cannot be used as a standalone expression");
            break;
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindAwaitExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::AwaitExpression:
        {
            // NS304-T05-F03: `await operand` resuelve el Future opaco del operando a su tipo
            // interno. El operando debe ser un Future (future:<T>). El registro de la suspension
            // (estado de maquina) y la transformacion HIR/MIR que la consume son F03.
            const ExpressionResult operand = analyzeExpression(
                expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (!operand.valid)
            {
                break;
            }
            const std::optional<std::string_view> inner =
                language::futureInnerTypeKey(operand.typeKey);
            if (!inner.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "await operand must be a Future value (future:<T>)");
                break;
            }
            result.typeKey = std::string(*inner);
            result.valid = true;
            awaitExpressions_.push_back(expression);
            break;
        }
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindIdentifierExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::IdentifierExpression:
            if (expression->auxiliaryText() == "generic-lambda-self" &&
                !activeLambdaBindings_.empty())
            {
                const MethodLambdaBinding& active = *activeLambdaBindings_.back();
                language::AnonymousLambdaType self;
                self.ownerOrdinal = active.ownerMemberOrdinal;
                self.lambdaOrdinal = active.lambdaOrdinal;
                self.signature.parameters = active.parameterTypeKeys;
                self.signature.result = active.returnTypeKey;
                self.signature.carrier = language::FunctionCarrier::Pointer;
                self.signature.callingConvention = language::CallingConvention::NativeSharp;
                result.typeKey = language::anonymousLambdaTypeIdentity(self);
                result.callableTrait = active.callableTrait;
                result.valid = !active.returnTypeKey.empty();
                result.assignable = false;
            }
            else
            {
                result = analyzeIdentifier(
                    expression, methodMemberOrdinal, scopeOrdinal, diagnostics, allowMethodGroup,
                    allowEvent, allowConstructorPropertyWrite);
            }
            break;
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindThisExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::ThisExpression:
            if (!deferCaptureAllowsThis())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "defer body uses this without an explicit 'ref this' capture");
                break;
            }
            if (!contextHasImplicitThis(methodMemberOrdinal))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidMemberAccess,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "this is not available in a static member or field initializer");
                break;
            }
            result.typeKey = implicitThisTypeKey(methodMemberOrdinal);
            result.valid = !isErrorType(result.typeKey);
            result.receiverSyntax = expression;
            result.receiverTypeKey = result.typeKey;
            result.receiverTypeOrdinal = referenceTypeOrdinalForTypeKey(result.typeKey);
            result.readonlyReceiver = contextHasReadonlyReceiver(methodMemberOrdinal);
            recordNameBinding(MethodNameBinding{
                MethodNameBindingKind::ThisReference,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                "this",
                result.typeKey,
                std::nullopt,
                std::nullopt,
                {},
                expression,
                result.typeKey,
                result.receiverTypeOrdinal,
                false});
            break;
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindBaseExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::BaseExpression:
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidMemberAccess,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                contextHasImplicitThis(methodMemberOrdinal)
                    ? "base may only appear as the receiver of a member access"
                    : "base is not available in a static member or field initializer");
            break;
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindIntegerLiteralExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::IntegerLiteralExpression:
            result.typeKey = integerLiteralType(expression);
            result.valid = true;
            result.integerConstant = ProjectIntegerConstant{
                expression->integerValue(),
                isUnsignedIntegerLiteral(expression)};
            break;
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindFloatingLiteralExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::FloatingLiteralExpression:
            // NS304-T15-F02: an `m`/`M` suffix is an exact decimal literal; otherwise the value is
            // a binary FP literal (float with `f`/`F`, double without suffix).
            if (!expression->text().empty() &&
                (expression->text().back() == 'm' || expression->text().back() == 'M'))
            {
                result.typeKey = "builtin:decimal";
            }
            else
            {
                result.typeKey = (!expression->text().empty() &&
                    (expression->text().back() == 'f' || expression->text().back() == 'F'))
                    ? "builtin:float"
                    : "builtin:double";
            }
            result.valid = true;
            break;
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindCharacterLiteralExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::CharacterLiteralExpression:
            result.typeKey = "builtin:char";
            result.valid = true;
            break;
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindOverflowPolicyExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::OverflowPolicyExpression:
        {
            const std::string policy = expression->auxiliaryText();
            if (policy == "unchecked" || policy == "wrapping")
            {
                // NS304-T15-F05: decimal has no modular arithmetic, so these two policies are
                // rejected outright (they would silently turn overflow into wrapping).
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "overflow policy '" + policy + "' is not allowed for decimal arithmetic");
                break;
            }
            if (policy != "checked" && policy != "saturating")
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "unsupported overflow policy '" + policy + "'");
                break;
            }
            if (expression->first() == nullptr)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "overflow policy expression requires an operand");
                break;
            }
            result = analyzeExpression(
                expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics,
                allowMethodGroup, allowEvent, allowMove, allowConstructorPropertyWrite);
            break;
        }
        default:
            break;
    }
    return result;
}

std::span<ast::TypeSyntax* const> ProjectMethodBodyModel::nameOfGenericArguments(
    const ast::AstNode* expression) const noexcept
{
    std::span<ast::TypeSyntax* const> genericArguments = expression->typeSyntaxes();
    if (genericArguments.empty() && expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::CallExpression)
    {
        genericArguments = expression->first()->typeSyntaxes();
    }
    return genericArguments;
}

bool ProjectMethodBodyModel::resolveNameOfTypeCandidate(
    const ast::AstNode* expression,
    const ast::AstNode* target,
    std::span<ast::TypeSyntax* const> genericArguments,
    bool& typeCandidateFound,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics) const
{
    if (nameLookup_ == nullptr) return false;
    if (nameLookup_->resolvedTypeOrdinal(expression->typeSyntax()).has_value())
    {
        typeCandidateFound = true;
        return true;
    }
    const ExpressionTypeNameBinding* type = nameLookup_->expressionTypeBinding(target);
    if (type == nullptr || type->candidateTypeOrdinals.empty()) return false;
    typeCandidateFound = true;
    if (type->candidateTypeOrdinals.size() != 1U)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::AmbiguousTypeName,
            diagnostics::DiagnosticSeverity::Error,
            target->range(),
            "ambiguous type name '" + type->writtenName + "'; candidates: " +
                typeCandidateList(type->candidateTypeOrdinals, *nameLookup_));
        return false;
    }
    const std::size_t typeOrdinal = type->candidateTypeOrdinals.front();
    const std::size_t expected = typeOrdinal < nameLookup_->namedTypes().size()
        ? nameLookup_->namedTypes()[typeOrdinal].identity.arity() : 0U;
    if (!genericArguments.empty() && expected != genericArguments.size())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "type '" + expression->text() + "' expects " + std::to_string(expected) +
                " generic argument(s) but received " +
                std::to_string(genericArguments.size()));
        return false;
    }
    return true;
}

std::optional<std::size_t> ProjectMethodBodyModel::nameOfOwnerTypeOrdinal(
    const ast::AstNode* target,
    [[maybe_unused]] std::size_t methodMemberOrdinal) const
{
    if (target->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        if (const ClassMemberSymbol* caller = methodSymbol(methodMemberOrdinal))
            return caller->declaringTypeOrdinal;
    }
    else if (target->kind() == ast::AstNodeKind::MemberExpression &&
             target->first() != nullptr)
    {
        if (const ExpressionTypeNameBinding* receiver =
                nameLookup_->expressionTypeBinding(target->first());
            receiver != nullptr && receiver->candidateTypeOrdinals.size() == 1U)
        {
            return receiver->candidateTypeOrdinals.front();
        }
    }
    return std::nullopt;
}

bool ProjectMethodBodyModel::resolveNameOfConcreteGenericMethod(
    const ast::AstNode* expression,
    std::size_t ownerTypeOrdinal,
    std::size_t accessingTypeOrdinal,
    std::size_t genericArgumentCount,
    bool& genericMethodNameFound) const
{
    for (const std::size_t ordinal :
         memberModel_->findMembers(ownerTypeOrdinal, expression->text()))
    {
        if (ordinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& method = memberModel_->members()[ordinal];
        if (method.kind != ClassMemberKind::Method || method.syntax == nullptr) continue;
        if (!ProjectMemberAccessibility::isAccessible(
                method, accessingTypeOrdinal, ownerTypeOrdinal,
                *nameLookup_, *inheritanceModel_)) continue;
        genericMethodNameFound = true;
        if (method.syntax->genericParameters().size() == genericArgumentCount) return true;
    }
    return false;
}

bool ProjectMethodBodyModel::resolveNameOfTemplateGenericMethod(
    const ast::AstNode* expression,
    std::size_t ownerTypeOrdinal,
    const ClassMemberSymbol* caller,
    std::size_t genericArgumentCount,
    bool& genericMethodNameFound) const
{
    if (genericModel_ == nullptr || ownerTypeOrdinal >= nameLookup_->namedTypes().size())
        return false;
    const NamedTypeDeclaration& owner = nameLookup_->namedTypes()[ownerTypeOrdinal];
    std::string ownerName;
    for (const compilation::TypeNameSegment& segment : owner.identity.containingTypes())
    {
        if (!ownerName.empty()) ownerName += "+";
        ownerName += segment.name;
    }
    if (!ownerName.empty()) ownerName += "+";
    ownerName += owner.identity.name();
    std::string namespaceName;
    for (const std::string& segment : owner.identity.namespaceSegments())
    {
        if (!namespaceName.empty()) namespaceName += ".";
        namespaceName += segment;
    }
    for (const GenericTemplateRecord& method : genericModel_->templates())
    {
        if (method.kind != GenericTemplateKind::Method ||
            method.projectOrdinal != owner.projectOrdinal ||
            method.namespaceName != namespaceName || method.ownerName != ownerName ||
            method.name != expression->text()) continue;
        genericMethodNameFound = true;
        const bool sameOwner =
            caller != nullptr && caller->declaringTypeOrdinal == ownerTypeOrdinal;
        if (!sameOwner && method.accessibility != GenericTemplateAccessibility::Public)
            continue;
        if (method.parameters.size() == genericArgumentCount) return true;
    }
    return false;
}

bool ProjectMethodBodyModel::resolveNameOfValueSymbol(
    const ast::AstNode* expression,
    const ast::AstNode* target,
    std::span<ast::TypeSyntax* const> genericArguments,
    bool genericMethodNameFound,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult symbol;
    if (!genericMethodNameFound)
    {
        const OwnershipFlowSnapshot beforeOperand = captureOwnershipFlow();
        const bool previousUnevaluated = inUnevaluatedOperand_;
        inUnevaluatedOperand_ = true;
        symbol = analyzeExpression(
            target, methodMemberOrdinal, scopeOrdinal, diagnostics,
            /*allowMethodGroup=*/true,
            /*allowEvent=*/true);
        inUnevaluatedOperand_ = previousUnevaluated;
        restoreOwnershipFlow(beforeOperand);
    }
    if (genericMethodNameFound)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "no symbol named '" + expression->text() + "' accepts " +
                std::to_string(genericArguments.size()) + " generic argument(s)");
        return false;
    }
    if (symbol.methodGroup && !genericArguments.empty())
    {
        const bool resolved = std::ranges::any_of(
            symbol.methodCandidates,
            [&](std::size_t ordinal)
            {
                const ClassMemberSymbol* method = methodSymbol(ordinal);
                return method != nullptr && method->syntax != nullptr &&
                    method->syntax->genericParameters().size() == genericArguments.size();
            });
        if (!resolved)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArgumentCount,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "no symbol named '" + expression->text() + "' accepts " +
                    std::to_string(genericArguments.size()) + " generic argument(s)");
        }
        return resolved;
    }
    if (!genericArguments.empty() && symbol.valid && !symbol.methodGroup)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "symbol '" + expression->text() + "' does not accept generic arguments");
        return false;
    }
    return symbol.valid || symbol.methodGroup;
}

ProjectMethodBodyModel::ExpressionResult
ProjectMethodBodyModel::analyzeExpressionKindNameOfExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    const ast::AstNode* target = expression->first();
    while (target != nullptr &&
           (target->kind() == ast::AstNodeKind::CallExpression ||
            target->kind() == ast::AstNodeKind::IndexExpression))
    {
        target = target->first();
    }
    if (target == nullptr || expression->text().empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "nameof requires a symbol, member, or generic name");
        return result;
    }
    const std::span<ast::TypeSyntax* const> genericArguments =
        nameOfGenericArguments(expression);
    bool typeCandidateFound = false;
    bool resolved = resolveNameOfTypeCandidate(
        expression, target, genericArguments, typeCandidateFound, diagnostics);
    if (!typeCandidateFound)
    {
        bool genericMethodNameFound = false;
        if (!genericArguments.empty() && memberModel_ != nullptr &&
            nameLookup_ != nullptr && inheritanceModel_ != nullptr)
        {
            const std::optional<std::size_t> ownerTypeOrdinal =
                nameOfOwnerTypeOrdinal(target, methodMemberOrdinal);
            if (ownerTypeOrdinal.has_value())
            {
                const ClassMemberSymbol* caller = methodSymbol(methodMemberOrdinal);
                const std::size_t accessingTypeOrdinal = caller == nullptr
                    ? *ownerTypeOrdinal : caller->declaringTypeOrdinal;
                resolved = resolveNameOfConcreteGenericMethod(
                    expression, *ownerTypeOrdinal, accessingTypeOrdinal,
                    genericArguments.size(), genericMethodNameFound);
                if (!resolved)
                {
                    resolved = resolveNameOfTemplateGenericMethod(
                        expression, *ownerTypeOrdinal, caller,
                        genericArguments.size(), genericMethodNameFound);
                }
            }
        }
        if (!resolved)
        {
            resolved = resolveNameOfValueSymbol(
                expression, target, genericArguments, genericMethodNameFound,
                methodMemberOrdinal, scopeOrdinal, diagnostics);
        }
    }
    if (!resolved) return result;
    result.typeKey = "string-literal";
    result.valid = true;
    coreBindings_.push_back(MethodCoreBinding{
        MethodCoreOperationKind::StringLiteral,
        expression,
        methodMemberOrdinal,
        scopeOrdinal,
        "builtin:string",
        "builtin:char",
        false, std::nullopt, std::nullopt});
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindStringLiteralExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::StringLiteralExpression:
            if (expression->tokenKind() == lexer::TokenKind::Utf8StringLiteral)
            {
                // NS304-T10-F01: `u8"..."` is a typed UTF-8 literal — a ReadOnlySpan<byte> over the
                // literal's UTF-8 bytes, emitted as a compile-time constant (no runtime heap).
                const std::string spanKey = genericModel_ != nullptr
                    ? genericModel_->closedTypeKeyForTemplate("ReadOnlySpan", "builtin:byte")
                    : std::string{};
                if (spanKey.empty())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidOperand,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                        "u8 literal requires NativeSharp.Core.ReadOnlySpan");
                    return result;
                }
                result.typeKey = spanKey;
                result.valid = true;
                coreBindings_.push_back(MethodCoreBinding{
                    MethodCoreOperationKind::Utf8StringLiteral,
                    expression,
                    methodMemberOrdinal,
                    scopeOrdinal,
                    spanKey,
                    "builtin:byte",
                    false, std::nullopt, std::nullopt});
                break;
            }
            result.typeKey = "string-literal";
            result.valid = true;
            coreBindings_.push_back(MethodCoreBinding{
                MethodCoreOperationKind::StringLiteral,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                "builtin:string",
                "builtin:char",
                false, std::nullopt, std::nullopt});
            break;
        default:
            break;
    }
    return result;
}

std::optional<std::size_t> ProjectMethodBodyModel::interpolationHandlerMember(
    std::size_t typeOrdinal,
    std::string_view name) const
{
    for (const std::size_t ordinal : memberModel_->findMembers(typeOrdinal, name))
    {
        if (ordinal < memberModel_->members().size()) return ordinal;
    }
    return std::nullopt;
}

bool ProjectMethodBodyModel::bindInterpolationHandler(
    ExpressionResult& result,
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    std::string_view handlerTarget,
    std::size_t typeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    const std::optional<std::size_t> appendLiteral =
        interpolationHandlerMember(typeOrdinal, "AppendLiteral");
    const std::optional<std::size_t> appendFormatted =
        interpolationHandlerMember(typeOrdinal, "AppendFormatted");
    const std::optional<std::size_t> finish =
        interpolationHandlerMember(typeOrdinal, "Finish");
    if (!appendLiteral.has_value() || !appendFormatted.has_value() || !finish.has_value())
        return false;
    const ClassMemberSymbol& formattedMember = memberModel_->members()[*appendFormatted];
    bool holesValid = formattedMember.parameters.size() == 1U;
    if (!holesValid)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "an interpolation handler AppendFormatted must take exactly one parameter");
    }
    if (holesValid)
    {
        // NS304-T10-F03: analyze each hole once, in source order, so the HIR
        // lowers it with its real type (mirrors the collection builder element
        // pass). The parser already retained the holes as the node children.
        for (const ast::AstNode* hole : expression->children())
        {
            if (hole == nullptr) continue;
            ExpressionResult holeValue = analyzeExpression(
                hole, methodMemberOrdinal, scopeOrdinal, diagnostics,
                false, false, true);
            if (!holeValue.valid)
            {
                holesValid = false;
                break;
            }
        }
    }
    result.typeKey = handlerTarget;
    result.valid = holesValid;
    if (result.valid)
    {
        MethodCoreBinding binding;
        binding.kind = MethodCoreOperationKind::InterpolationHandler;
        binding.syntax = expression;
        binding.methodMemberOrdinal = methodMemberOrdinal;
        binding.scopeOrdinal = scopeOrdinal;
        binding.resultTypeKey = handlerTarget;
        binding.builderMemberOrdinals = {*appendLiteral, *appendFormatted, *finish};
        coreBindings_.push_back(std::move(binding));
    }
    return true;
}

ProjectMethodBodyModel::ExpressionResult
ProjectMethodBodyModel::analyzeExpressionKindInterpolatedStringExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    // NS304-T10-F03: an interpolation targeting a handler type lowers to
    // AppendLiteral(parts) + AppendFormatted(holes) + Finish(), with no temporary
    // concatenation. Only the fully-static form falls through to the consteval handler.
    const std::string handlerTarget = activeTargetTypeKey_;
    if (!handlerTarget.empty() && inheritanceModel_ != nullptr && memberModel_ != nullptr)
    {
        const std::optional<std::size_t> typeOrdinal =
            inheritanceModel_->typeOrdinalForTypeKey(handlerTarget);
        if (typeOrdinal.has_value() && bindInterpolationHandler(
                result, expression, methodMemberOrdinal, scopeOrdinal,
                handlerTarget, *typeOrdinal, diagnostics))
        {
            return result;
        }
    }
    // T01-F06 first allocation-free handler: every hole is a compile-time scalar and the
    // complete answer is interned exactly like a source string literal. Runtime holes are
    // rejected instead of silently selecting String.Concat (which would allocate heap in
    // a `noheap` context). Later handler protocols may widen this without changing the
    // representation of this fully-static form.
    std::vector<language::InterpolationPart> parts;
    std::string parseError;
    if (!language::parseStaticInterpolation(expression->text(), parts, parseError))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidStringLiteral,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            std::move(parseError));
        return result;
    }
    std::string rendered;
    std::string renderError;
    if (!language::renderBuiltinStaticInterpolation(parts, rendered, renderError))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidStringLiteral,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            std::move(renderError));
        return result;
    }
    staticInterpolationValues_[expression] = std::move(rendered);
    result.typeKey = "string-literal";
    result.valid = true;
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindBooleanLiteralExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::BooleanLiteralExpression:
            result.typeKey = "builtin:bool";
            result.valid = true;
            break;
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindNullLiteralExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::NullLiteralExpression:
            result.typeKey = "builtin:null";
            result.valid = true;
            break;
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindLambdaExpression(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics,
    [[maybe_unused]] bool allowMethodGroup,
    [[maybe_unused]] bool allowEvent,
    [[maybe_unused]] bool allowMove,
    [[maybe_unused]] bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::LambdaExpression:
            result = bindLambdaToDelegate(
                expression, {}, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        default:
            break;
    }
    return result;
}
}
