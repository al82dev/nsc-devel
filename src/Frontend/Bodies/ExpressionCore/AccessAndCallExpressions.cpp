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

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindNullConditionalMemberExpression(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup,
    bool allowEvent,
    bool allowMove,
    bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::NullConditionalMemberExpression:
        {
            result = analyzeMember(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics,
                allowMethodGroup, false, false);
            if (!result.valid) break;
            const std::optional<std::size_t> receiverOrdinal =
                namedTypeOrdinalForTypeKey(result.receiverTypeKey);
            const bool nullableReceiver = receiverOrdinal.has_value() && inheritanceModel_ != nullptr &&
                (inheritanceModel_->isClass(*receiverOrdinal) ||
                 (interfaceModel_ != nullptr && interfaceModel_->isInterface(*receiverOrdinal)));
            if (!nullableReceiver)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->first() != nullptr ? expression->first()->range() : expression->range(),
                    "the receiver of '?.' must be a class or interface reference");
                result.valid = false;
                break;
            }
            // A method group has no result type until overload resolution runs in analyzeCall.
            // Keep it alive here and validate the selected method's result at the end of that
            // routine.  Arguments remain semantic branches and the backend keeps them off the
            // null edge through the nullConditionalCall bit.
            if (result.methodGroup)
            {
                result.assignable = false;
                result.addressable = false;
                break;
            }
            const bool nullableResult = ProjectTypeConversion::isPointer(result.typeKey) ||
                ProjectTypeConversion::isNativeArray(result.typeKey) ||
                result.typeKey == "builtin:string" || isAnyDelegateTypeKey(result.typeKey) ||
                [&]
                {
                    const std::optional<std::size_t> ordinal =
                        namedTypeOrdinalForTypeKey(result.typeKey);
                    return ordinal.has_value() && inheritanceModel_ != nullptr &&
                        (inheritanceModel_->isClass(*ordinal) ||
                         (interfaceModel_ != nullptr && interfaceModel_->isInterface(*ordinal)));
                }();
            if (!nullableResult && !activeNullConditionalWrite_)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "this '?.' access requires a nullable field/property/method result; value "
                    "results require the Optional<T> carrier");
                result.valid = false;
                break;
            }
            result.assignable = false;
            result.addressable = false;
            break;
        }
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindMemberExpression(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup,
    bool allowEvent,
    bool allowMove,
    bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::MemberExpression:
            result = analyzeMember(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics, allowMethodGroup, allowEvent,
                allowConstructorPropertyWrite);
            break;
        // IDs 80 y 83. `p->campo` y `p->Metodo()` ya no llegan aqui: el parser los convierte en
        // `(*p).campo` y `(*p).Metodo()`, que es la MISMA operacion y la que siempre funciono.
        // Ver Parser::parsePostfixExpression. El caso se queda -- sin rechazo -- porque el nodo
        // sigue existiendo en el AST y un `default` mudo seria peor que una ruta explicita.
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindPointerMemberExpression(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup,
    bool allowEvent,
    bool allowMove,
    bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::PointerMemberExpression:
            result = analyzeMember(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics, allowMethodGroup, allowEvent,
                allowConstructorPropertyWrite);
            break;
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindCallExpression(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup,
    bool allowEvent,
    bool allowMove,
    bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::CallExpression:
            result = analyzeCall(expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        default:
            break;
    }
    return result;
}

bool ProjectMethodBodyModel::resolveResultPropagationErrorConversion(
    const MethodResultPropagationBinding& source,
    const MethodResultPropagationBinding& destination,
    const ast::AstNode* expression,
    std::optional<std::size_t>& errorConversion,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (source.sourceErrorTypeKey == destination.sourceErrorTypeKey) return true;
    if (inheritanceModel_ == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "Result propagation cannot resolve the declared error conversion");
        return false;
    }
    const ProjectConversionClassifier classifier(
        *inheritanceModel_, interfaceModel_, memberModel_);
    const ProjectImplicitConversion conversion = classifier.classifyExplicitUserDefined(
        source.sourceErrorTypeKey, destination.sourceErrorTypeKey);
    if (conversion.kind != ProjectConversionKind::UserDefinedExplicit ||
        !conversion.userDefinedMemberOrdinal.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "postfix '?' requires error type identity or a declared explicit conversion from '" +
                source.sourceErrorTypeKey + "' to '" + destination.sourceErrorTypeKey + "'");
        return false;
    }
    errorConversion = conversion.userDefinedMemberOrdinal;
    return true;
}

void ProjectMethodBodyModel::recordResultPropagationBinding(
    MethodResultPropagationBinding source,
    const MethodResultPropagationBinding& destination,
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    std::string_view returnTypeKey,
    std::optional<std::size_t> errorConversion,
    ExpressionResult& result)
{
    source.syntax = expression;
    source.operandSyntax = expression->first();
    source.methodMemberOrdinal = methodMemberOrdinal;
    source.scopeOrdinal = scopeOrdinal;
    source.returnCarrierTypeKey = returnTypeKey;
    source.returnErrorTypeKey = destination.sourceErrorTypeKey;
    source.returnErrorTag = destination.sourceErrorTag;
    source.returnErrorFactoryMemberOrdinal = destination.returnErrorFactoryMemberOrdinal;
    source.errorConversionMemberOrdinal = errorConversion;
    resultPropagationBindings_.push_back(std::move(source));
    recordStructuredExit(
        MethodStructuredExitKind::Return,
        expression,
        methodMemberOrdinal,
        scopeOrdinal,
        std::nullopt,
        0);
    result.typeKey = resultPropagationBindings_.back().successTypeKey;
    result.valid = true;
    result.assignable = false;
    result.addressable = false;
    if (memberModel_ != nullptr && memberModel_->typeIsMoveOnly(result.typeKey))
    {
        result.ownership = sema::OwnershipKind::Owned;
        result.ownershipTransfer = true;
    }
}

bool ProjectMethodBodyModel::validateResultPropagationContext(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    std::string& returnTypeKey)
{
    if (!finallyStack_.empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidReturn,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "Result propagation cannot leave a finally block");
        return false;
    }
    const ClassMemberSymbol* callable = methodSymbol(methodMemberOrdinal);
    returnTypeKey = currentLambdaOrdinal_.has_value()
        ? currentLambdaReturnTypeKey_
        : callable != nullptr ? callable->declaredTypeKey : std::string("<error>");
    if (callable != nullptr && callable->kind != ClassMemberKind::Constructor &&
        callable->kind != ClassMemberKind::Destructor && returnTypeKey != "builtin:void")
    {
        return true;
    }
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidReturn,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "Result propagation requires a value-returning callable");
    return false;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindResultPropagationExpression(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup,
    bool allowEvent,
    bool allowMove,
    bool allowConstructorPropertyWrite)
{
    ExpressionResult result;
    switch (expression->kind())
    {
        case ast::AstNodeKind::ResultPropagationExpression:
        {
            std::string returnTypeKey;
            if (!validateResultPropagationContext(
                    expression, methodMemberOrdinal, diagnostics, returnTypeKey))
            {
                break;
            }

            const ExpressionResult operand = analyzeExpression(
                expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics,
                false, false, true);
            if (!operand.valid) break;

            std::optional<MethodResultPropagationBinding> source =
                resolveResultCarrier(operand.typeKey);
            if (!source.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->first() != nullptr
                        ? expression->first()->range() : expression->range(),
                    "operand of postfix '?' must be a two-case variant with exactly "
                    "Ok(value) and Error(error) cases");
                break;
            }

            std::optional<MethodResultPropagationBinding> destination =
                resolveResultCarrier(returnTypeKey);
            if (!destination.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidReturn,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "a callable using postfix '?' must return a compatible Ok/Error variant");
                break;
            }

            if (memberModel_ != nullptr && memberModel_->typeIsMoveOnly(operand.typeKey) &&
                operand.addressable && !operand.ownershipTransfer)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->first() != nullptr
                        ? expression->first()->range() : expression->range(),
                    "postfix '?' consumes a move-only Result carrier; write move(result)?");
                break;
            }

            std::optional<std::size_t> errorConversion;
            if (!resolveResultPropagationErrorConversion(
                    *source, *destination, expression, errorConversion, diagnostics))
            {
                break;
            }
            recordResultPropagationBinding(
                std::move(*source), *destination, expression, methodMemberOrdinal,
                scopeOrdinal, returnTypeKey, errorConversion, result);
            break;
        }
        default:
            break;
    }
    return result;
}
}
