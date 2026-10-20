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

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindAssignmentExpression(
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
        case ast::AstNodeKind::AssignmentExpression:
            result = analyzeAssignment(expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
            break;
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult
ProjectMethodBodyModel::analyzeExpressionKindTypeTestExpressionOrSafeCastExpression(
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
        case ast::AstNodeKind::TypeTestExpression:
        case ast::AstNodeKind::SafeCastExpression:
        {
            const ExpressionResult operand = analyzeExpression(
                expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (memberModel_ == nullptr || nameLookup_ == nullptr)
            {
                break;
            }
            const std::string targetTypeKey =
                memberModel_->canonicalTypeKey(expression->typeSyntax(), *nameLookup_);
            const std::optional<std::size_t> targetType =
                referenceTypeOrdinalForTypeKey(targetTypeKey);
            const bool sourceIsReference = operand.typeKey == "builtin:null" ||
                isReferenceTypeKey(operand.typeKey);
            if (!operand.valid || isErrorType(targetTypeKey))
            {
                break;
            }
            if (!targetType.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    std::string(expression->kind() == ast::AstNodeKind::TypeTestExpression ? "is" : "as") +
                        " target must be a class or interface reference type");
                break;
            }
            if (!sourceIsReference || !canRuntimeReferenceConvert(operand.typeKey, targetTypeKey))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "no runtime reference conversion exists from '" + operand.typeKey +
                        "' to '" + targetTypeKey + "'");
                break;
            }

            result.typeKey = expression->kind() == ast::AstNodeKind::TypeTestExpression
                ? "builtin:bool"
                : targetTypeKey;
            result.valid = true;
            if (expression->kind() == ast::AstNodeKind::SafeCastExpression)
            {
                result.objectLifetimeOwnership = operand.objectLifetimeOwnership;
                result.storageOwnership = operand.storageOwnership;
            }
            runtimeTypeBindings_.push_back(MethodRuntimeTypeBinding{
                expression->kind() == ast::AstNodeKind::TypeTestExpression
                    ? MethodRuntimeTypeOperationKind::TypeTest
                    : MethodRuntimeTypeOperationKind::SafeCast,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                operand.typeKey,
                targetTypeKey,
                *targetType,
                interfaceModel_ != nullptr && interfaceModel_->isInterface(*targetType)});
            break;
        }
        default:
            break;
    }
    return result;
}

bool ProjectMethodBodyModel::validateNamedCast(
    ExpressionResult& result,
    const ExpressionResult& operand,
    const ast::AstNode* expression,
    std::string_view namedCast,
    bool targetPointeeReadonly,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics) const
{
    const bool sourcePointer = ProjectTypeConversion::isPointer(operand.typeKey);
    const bool targetPointer = ProjectTypeConversion::isPointer(result.typeKey);
    bool allowed = false;
    if (namedCast == "const_cast")
    {
        // Qualifiers are source-only and ABI-neutral in this 3.0 profile. The
        // base/indirection identity must therefore remain byte-for-byte equal.
        allowed = sourcePointer && targetPointer && operand.typeKey == result.typeKey;
        if (allowed) result.pointerPointeeReadonly = targetPointeeReadonly;
    }
    else if (namedCast == "reinterpret_cast")
    {
        const bool sourceAddress =
            sourcePointer || isPointerSizedIntegralTypeKey(operand.typeKey);
        const bool targetAddress =
            targetPointer || isPointerSizedIntegralTypeKey(result.typeKey);
        allowed = sourceAddress && targetAddress &&
            !(operand.pointerPointeeReadonly && targetPointer && !targetPointeeReadonly);
    }
    else if (namedCast == "static_cast")
    {
        const bool numeric = ProjectTypeConversion::isNumeric(operand.typeKey) &&
            ProjectTypeConversion::isNumeric(result.typeKey);
        const bool enumNumeric =
            (isEnumTypeKey(operand.typeKey) &&
             ProjectTypeConversion::isIntegral(result.typeKey)) ||
            (ProjectTypeConversion::isIntegral(operand.typeKey) &&
             isEnumTypeKey(result.typeKey));
        const bool voidPointerRecovery = sourcePointer && targetPointer &&
            (operand.typeKey == "builtin:void*" || result.typeKey == "builtin:void*");
        const ProjectImplicitConversion implicit =
            classifyImplicit(operand.typeKey, result.typeKey, operand.integerConstant);
        allowed = operand.typeKey == result.typeKey || numeric || enumNumeric ||
            voidPointerRecovery || implicit.exists();
        if (operand.pointerPointeeReadonly && targetPointer && !targetPointeeReadonly)
            allowed = false;
    }
    if (allowed) return true;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidConversion,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        std::string(namedCast) + " cannot convert '" + operand.typeKey +
            "' to '" + result.typeKey + "' under its restricted contract");
    result.valid = false;
    return false;
}

bool ProjectMethodBodyModel::applyUserDefinedCast(
    ExpressionResult& result,
    const ExpressionResult& operand,
    const ast::AstNode* expression,
    const ProjectConversionClassifier& classifier,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    const ProjectImplicitConversion userConversion =
        classifier.classifyExplicitUserDefined(operand.typeKey, result.typeKey);
    if ((userConversion.kind != ProjectConversionKind::UserDefinedImplicit &&
         userConversion.kind != ProjectConversionKind::UserDefinedExplicit) ||
        !userConversion.userDefinedMemberOrdinal.has_value()) return false;
    const std::vector<ExpressionResult> arguments = {operand};
    ExpressionResult converted = resolveSyntheticCall(
        expression,
        methodMemberOrdinal,
        scopeOrdinal,
        userConversion.kind == ProjectConversionKind::UserDefinedImplicit
            ? "op_Implicit" : "op_Explicit",
        {*userConversion.userDefinedMemberOrdinal},
        arguments,
        nullptr,
        {},
        std::nullopt,
        false,
        false,
        diagnostics);
    if (converted.valid) result = converted;
    return true;
}

bool ProjectMethodBodyModel::validateFunctionPointerCast(
    ExpressionResult& result,
    const ExpressionResult& operand,
    const ast::AstNode* expression,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics) const
{
    // C1.1 "function-pointer round trip". A delegate type key is neither a class
    // nor an interface, so before this existed NOTHING classified a cast that
    // touched one: `(nint)fp`, `(Fn)raw` and `(Other)fp` all sailed through the
    // frontend untyped and died in the backend as "Stage 34 local initialization
    // failed" (measured). The round trip is now a first-class, explicitly
    // classified conversion in BOTH directions, and the cases that are NOT part of
    // it get a real diagnostic here instead of a backend crash:
    //
    //   extern C delegate <-> nint/nuint      address in, address out
    //   extern C delegate <-> T* / void*      the GetProcAddress shape
    //   extern C delegate <-> extern C delegate   explicit reinterpret between
    //                                             two foreign signatures
    //
    // Why the reinterpret is EXPLICIT-only and never implicit: on Win64 there is
    // exactly one native C calling convention, so the convention is fixed by
    // `extern C` itself and is part of the type -- what distinguishes two extern C
    // delegate types is their SIGNATURE, and reinterpreting one signature as
    // another is precisely the unchecked act `wglGetProcAddress` forces on every
    // caller. Requiring the cast keeps that act visible in the source.
    //
    // An ORDINARY delegate stays out of all of this: it is a 16-byte heap cell
    // (code + receiver), not an address, so casting one to an integer or pointer
    // would hand out the cell -- exactly the miscompile Stage 55.x measured on the
    // creation side (a cell pointer handed to C, invoked, exit 5 for an answer of
    // 42). It is rejected by name.
    const bool sourceIsExternCFunctionPointer = isExternCDelegateTypeKey(operand.typeKey);
    const bool targetIsExternCFunctionPointer = isExternCDelegateTypeKey(result.typeKey);
    const bool sourceIsAddressLike = sourceIsExternCFunctionPointer ||
        isRawPointerTypeKey(operand.typeKey) ||
        isPointerSizedIntegralTypeKey(operand.typeKey);
    const bool targetIsAddressLike = targetIsExternCFunctionPointer ||
        isRawPointerTypeKey(result.typeKey) ||
        isPointerSizedIntegralTypeKey(result.typeKey);
    if (sourceIsExternCFunctionPointer || targetIsExternCFunctionPointer)
    {
        if (!sourceIsAddressLike || !targetIsAddressLike)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "an extern C function pointer converts only to and from another "
                "extern C function pointer, a raw pointer or nint/nuint; cannot "
                "convert '" + operand.typeKey + "' to '" + result.typeKey + "'");
            result.valid = false;
        }
        return true;
    }
    if (isAnyDelegateTypeKey(operand.typeKey) || isAnyDelegateTypeKey(result.typeKey))
    {
        if (operand.typeKey != result.typeKey &&
            (sourceIsAddressLike || targetIsAddressLike))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "an ordinary delegate is a code+receiver cell, not an address; "
                "declare the type 'extern C delegate' to convert between '" +
                    operand.typeKey + "' and '" + result.typeKey + "'");
            result.valid = false;
            return true;
        }
    }
    return false;
}

void ProjectMethodBodyModel::validateReferenceCast(
    ExpressionResult& result,
    const ExpressionResult& operand,
    const ast::AstNode* expression,
    const ProjectConversionClassifier& classifier,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    const bool sourceIsReference =
        operand.typeKey == "builtin:null" || isReferenceTypeKey(operand.typeKey);
    const std::optional<std::size_t> targetType =
        referenceTypeOrdinalForTypeKey(result.typeKey);
    const bool targetIsReference = targetType.has_value();
    if (!sourceIsReference && !targetIsReference) return;
    if (!sourceIsReference || !targetIsReference ||
        !canRuntimeReferenceConvert(operand.typeKey, result.typeKey))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "invalid explicit reference conversion from '" + operand.typeKey +
                "' to '" + result.typeKey + "'");
        result.valid = false;
        return;
    }
    const ProjectImplicitConversion implicit =
        classifier.classifyImplicit(operand.typeKey, result.typeKey, operand.integerConstant);
    if (!implicit.exists() && operand.typeKey != "builtin:null")
    {
        runtimeTypeBindings_.push_back(MethodRuntimeTypeBinding{
            MethodRuntimeTypeOperationKind::CheckedCast,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            operand.typeKey,
            result.typeKey,
            *targetType,
            interfaceModel_ != nullptr && interfaceModel_->isInterface(*targetType)});
    }
}

ProjectMethodBodyModel::ExpressionResult
ProjectMethodBodyModel::analyzeExpressionKindCastExpression(
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
    const ExpressionResult operand = analyzeExpression(
        expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
    if (memberModel_ == nullptr || nameLookup_ == nullptr) return result;
    result.typeKey = memberModel_->canonicalTypeKey(expression->typeSyntax(), *nameLookup_);
    result.valid = operand.valid && !isErrorType(result.typeKey);
    result.objectLifetimeOwnership = operand.objectLifetimeOwnership;
    result.storageOwnership = operand.storageOwnership;
    const bool targetPointeeReadonly =
        expression->typeSyntax() != nullptr && expression->typeSyntax()->isPointeeConst();
    const language::PointerContract targetPointerContract =
        pointerContractFromSyntax(expression->typeSyntax());
    result.pointerPointeeReadonly = targetPointeeReadonly;
    result.pointerNullable = targetPointerContract.nullable;
    result.pointerHasProvenance = targetPointerContract.hasProvenance;
    result.pointerKnownAlignment = targetPointerContract.knownAlignment;
    result.pointerAddressSpace = targetPointerContract.addressSpace;
    result.onePastEndPointer = operand.onePastEndPointer;
    const std::string_view namedCast = expression->auxiliaryText().starts_with("cast:")
        ? std::string_view(expression->auxiliaryText()).substr(5U)
        : std::string_view{};
    if (result.valid && !namedCast.empty() &&
        !validateNamedCast(
            result, operand, expression, namedCast, targetPointeeReadonly, diagnostics))
    {
        return result;
    }
    if (result.valid && namedCast.empty() && operand.pointerPointeeReadonly &&
        ProjectTypeConversion::isPointer(result.typeKey) && !targetPointeeReadonly)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "a C-style cast cannot discard pointer pointee constness; "
            "use const_cast<T*> explicitly");
        result.valid = false;
        return result;
    }
    if (!result.valid || inheritanceModel_ == nullptr) return result;
    const ProjectConversionClassifier classifier(
        *inheritanceModel_, interfaceModel_, memberModel_);
    if (applyUserDefinedCast(
            result, operand, expression, classifier,
            methodMemberOrdinal, scopeOrdinal, diagnostics))
    {
        return result;
    }
    if (validateFunctionPointerCast(result, operand, expression, diagnostics))
        return result;
    validateReferenceCast(
        result, operand, expression, classifier,
        methodMemberOrdinal, scopeOrdinal, diagnostics);
    return result;
}
}
