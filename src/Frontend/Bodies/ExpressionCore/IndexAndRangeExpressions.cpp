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

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindFromEndExpression(
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
        case ast::AstNodeKind::FromEndExpression:
        {
            const ExpressionResult distance = analyzeExpression(
                expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (!distance.valid) break;
            if (!ProjectTypeConversion::isIntegral(distance.typeKey) ||
                distance.typeKey == "builtin:bool")
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidIndex,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "a from-end index requires an integral distance");
                break;
            }
            if (distance.integerConstant.has_value() && distance.integerConstant->isNegative)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::ConstantIndexOutOfBounds,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "a from-end index distance cannot be negative");
                break;
            }
            result = distance;
            result.assignable = false;
            result.addressable = false;
            break;
        }
        default:
            break;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindRangeExpression(
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
        case ast::AstNodeKind::RangeExpression:
        {
            bool valid = true;
            const auto checkEndpoint = [&](const ast::AstNode* endpoint)
            {
                if (endpoint == nullptr) return;
                const ExpressionResult value = analyzeExpression(
                    endpoint, methodMemberOrdinal, scopeOrdinal, diagnostics);
                if (!value.valid || !ProjectTypeConversion::isIntegral(value.typeKey) ||
                    value.typeKey == "builtin:bool")
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidIndex,
                        diagnostics::DiagnosticSeverity::Error,
                        endpoint->range(),
                        "a range endpoint requires an integral index");
                    valid = false;
                }
            };
            checkEndpoint(expression->first());
            checkEndpoint(expression->second());
            checkEndpoint(expression->third());
            result.typeKey = "internal:range";
            result.valid = valid;
            break;
        }
        default:
            break;
    }
    return result;
}

void ProjectMethodBodyModel::analyzeIndexOperands(
    ExpressionResult& base,
    std::vector<ExpressionResult>& indices,
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    ++partialProjectionReceiverDepth_;
    base = analyzeExpression(expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
    --partialProjectionReceiverDepth_;
    const auto& indexSyntaxes = expression->children();
    if (!indexSyntaxes.empty())
    {
        indices.reserve(indexSyntaxes.size());
        for (const ast::AstNode* indexSyntax : indexSyntaxes)
        {
            indices.push_back(analyzeExpression(
                indexSyntax, methodMemberOrdinal, scopeOrdinal, diagnostics));
        }
    }
    else if (expression->second() != nullptr)
    {
        indices.push_back(analyzeExpression(
            expression->second(), methodMemberOrdinal, scopeOrdinal, diagnostics));
    }
}

bool ProjectMethodBodyModel::analyzeNativeArrayIndex(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ExpressionResult& base,
    const std::vector<ExpressionResult>& indices,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    if (!base.valid || !ProjectTypeConversion::isNativeArray(base.typeKey)) return false;
    const std::size_t arrayRank = ProjectTypeConversion::nativeArrayRank(base.typeKey);
    const auto& indexSyntaxes = expression->children();
    const ast::AstNode* singleIndex = !indexSyntaxes.empty()
        ? indexSyntaxes.front() : expression->second();
    if (indices.size() == 1 && singleIndex != nullptr &&
        singleIndex->kind() == ast::AstNodeKind::RangeExpression)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidIndex,
            diagnostics::DiagnosticSeverity::Error,
            singleIndex->range(),
            "slicing a heap array cannot implicitly allocate another array; use a "
            "Span<T>/ReadOnlySpan<T> Length/Slice view");
        return true;
    }
    bool validIndices = indices.size() == arrayRank;
    for (const ExpressionResult& index : indices)
        validIndices = validIndices && index.valid &&
            ProjectTypeConversion::isIntegral(index.typeKey) && index.typeKey != "builtin:bool";
    const std::optional<std::string> element = validIndices
        ? ProjectTypeConversion::nativeArrayElementType(base.typeKey) : std::nullopt;
    if (element.has_value())
    {
        result.typeKey = *element;
        result.valid = true;
        result.assignable = true;
        // An array element is real mutable storage. Item 329 made addressability
        // explicit so bit fields could reject `&field`; preserve the long-standing
        // `&array[index]` raw-pointer boundary as a separate property.
        result.addressable = true;
        coreBindings_.push_back(MethodCoreBinding{
            arrayRank > 1 ? MethodCoreOperationKind::RangeArrayElement
                          : MethodCoreOperationKind::ArrayElement,
            expression, methodMemberOrdinal, scopeOrdinal, result.typeKey,
            result.typeKey, true, std::nullopt, std::nullopt});
    }
    if (!result.valid)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidIndex,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "native array indexing requires exactly one integral index per dimension");
    }
    return true;
}

bool ProjectMethodBodyModel::analyzeNativeStringIndex(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ExpressionResult& base,
    const std::vector<ExpressionResult>& indices,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    if (!base.valid || (!ProjectTypeConversion::isNativeString(base.typeKey) &&
        base.typeKey != "string-literal")) return false;
    const auto& indexSyntaxes = expression->children();
    const ast::AstNode* singleIndex = !indexSyntaxes.empty()
        ? indexSyntaxes.front() : expression->second();
    if (indices.size() == 1 && singleIndex != nullptr &&
        singleIndex->kind() == ast::AstNodeKind::RangeExpression)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidIndex,
            diagnostics::DiagnosticSeverity::Error,
            singleIndex->range(),
            "slicing a native string cannot allocate implicitly; use StringView.Slice");
        return true;
    }
    if (indices.size() == 1 && indices.front().valid &&
        ProjectTypeConversion::isIntegral(indices.front().typeKey))
    {
        result.typeKey = "builtin:char";
        result.valid = true;
        result.assignable = false;
        coreBindings_.push_back(MethodCoreBinding{
            MethodCoreOperationKind::StringElement, expression, methodMemberOrdinal,
            scopeOrdinal, result.typeKey, "builtin:char", false, std::nullopt, std::nullopt});
    }
    else
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidIndex,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "native string indexing requires exactly one integral index");
    }
    return true;
}

std::vector<std::size_t> ProjectMethodBodyModel::rangeProtocolCandidates(
    std::size_t receiverType,
    std::size_t accessingTypeOrdinal,
    std::string_view name) const
{
    std::vector<std::size_t> candidates;
    const bool interfaceReceiver = interfaceModel_ != nullptr &&
        interfaceModel_->isInterface(receiverType);
    if (memberModel_ == nullptr || nameLookup_ == nullptr || inheritanceModel_ == nullptr ||
        interfaceReceiver)
    {
        return candidates;
    }
    for (const std::size_t ordinal : memberCandidatesForType(receiverType, name))
    {
        if (ordinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& member = memberModel_->members()[ordinal];
        if (member.kind != ClassMemberKind::Method || member.isStatic ||
            !ProjectMemberAccessibility::isAccessible(
                member, accessingTypeOrdinal, receiverType, *nameLookup_, *inheritanceModel_))
        {
            continue;
        }
        candidates.push_back(ordinal);
    }
    return candidates;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzedRangeEndpoint(
    const ast::AstNode* syntax,
    [[maybe_unused]] std::size_t methodMemberOrdinal) const
{
    ExpressionResult endpoint;
    if (syntax == nullptr) return endpoint;
    const MethodExpressionTypeBinding* binding =
        expressionTypeBinding(syntax, methodMemberOrdinal);
    if (binding != nullptr)
    {
        endpoint.typeKey = binding->typeKey;
        endpoint.integerConstant = binding->integerConstant;
        endpoint.valid = !isErrorType(endpoint.typeKey);
    }
    return endpoint;
}

bool ProjectMethodBodyModel::resolveRangeProtocol(
    ExpressionResult& sliceResult,
    MethodCallBinding& lengthCall,
    MethodCallBinding& sliceCall,
    const ast::AstNode* expression,
    const ExpressionResult& base,
    std::size_t receiverType,
    std::size_t accessingTypeOrdinal,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    const std::vector<ExpressionResult> noArguments;
    const ExpressionResult lengthResult = resolveSyntheticCall(
        expression, methodMemberOrdinal, scopeOrdinal, "Length",
        rangeProtocolCandidates(receiverType, accessingTypeOrdinal, "Length"), noArguments,
        expression->first(), base.typeKey, receiverType,
        false, false, diagnostics, false, base.readonlyReceiver);
    lengthCall = calls_.empty() ? MethodCallBinding{} : calls_.back();
    ExpressionResult integerArgument;
    integerArgument.typeKey = "builtin:int";
    integerArgument.valid = true;
    const std::vector<ExpressionResult> sliceArguments = {integerArgument, integerArgument};
    sliceResult = resolveSyntheticCall(
        expression, methodMemberOrdinal, scopeOrdinal, "Slice",
        rangeProtocolCandidates(receiverType, accessingTypeOrdinal, "Slice"), sliceArguments,
        expression->first(), base.typeKey, receiverType,
        false, false, diagnostics, false, base.readonlyReceiver);
    sliceCall = calls_.empty() ? MethodCallBinding{} : calls_.back();
    const bool validProtocol = lengthResult.valid && lengthResult.typeKey == "builtin:int" &&
        lengthCall.resolution == MethodCallResolutionKind::Resolved &&
        lengthCall.selectedMemberOrdinal.has_value() && sliceResult.valid &&
        sliceCall.resolution == MethodCallResolutionKind::Resolved &&
        sliceCall.selectedMemberOrdinal.has_value();
    if (validProtocol) return true;
    const auto& indexSyntaxes = expression->children();
    const ast::AstNode* rangeSyntax = !indexSyntaxes.empty()
        ? indexSyntaxes.front() : expression->second();
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidIndex,
        diagnostics::DiagnosticSeverity::Error,
        rangeSyntax->range(),
        "range indexing requires accessible instance methods 'int Length()' "
        "and 'Slice(int start, int length)'");
    return false;
}

bool ProjectMethodBodyModel::validateRangeProtocolMembers(
    const ast::AstNode* rangeSyntax,
    const MethodCallBinding& lengthCall,
    const MethodCallBinding& sliceCall,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics) const
{
    const ClassMemberSymbol& lengthMember =
        memberModel_->members()[*lengthCall.selectedMemberOrdinal];
    const ClassMemberSymbol& sliceMember =
        memberModel_->members()[*sliceCall.selectedMemberOrdinal];
    if (lengthMember.isReferenceReturn || sliceMember.isReferenceReturn)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidIndex,
            diagnostics::DiagnosticSeverity::Error,
            rangeSyntax->range(),
            "the Length/Slice range protocol methods must return by value");
        return false;
    }
    if (lengthMember.isVirtual || lengthMember.isOverride || lengthMember.isAbstract ||
        sliceMember.isVirtual || sliceMember.isOverride || sliceMember.isAbstract)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidIndex,
            diagnostics::DiagnosticSeverity::Error,
            rangeSyntax->range(),
            "the Length/Slice range protocol is statically dispatched; virtual, "
            "override, abstract and interface protocol members are not permitted");
        return false;
    }
    return true;
}

bool ProjectMethodBodyModel::validateRangeEndpoints(
    ProjectImplicitConversion& startConversion,
    ProjectImplicitConversion& endConversion,
    ExpressionResult& startResult,
    ExpressionResult& endResult,
    const ast::AstNode* rangeSyntax,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics) const
{
    const ast::AstNode* writtenStart = rangeSyntax->first();
    const ast::AstNode* writtenEnd = rangeSyntax->second();
    const bool startFromEnd = writtenStart != nullptr &&
        writtenStart->kind() == ast::AstNodeKind::FromEndExpression;
    const bool endFromEnd = writtenEnd != nullptr &&
        writtenEnd->kind() == ast::AstNodeKind::FromEndExpression;
    const ast::AstNode* startValue = startFromEnd ? writtenStart->first() : writtenStart;
    const ast::AstNode* endValue = endFromEnd ? writtenEnd->first() : writtenEnd;
    startResult = analyzedRangeEndpoint(startValue, methodMemberOrdinal);
    endResult = analyzedRangeEndpoint(endValue, methodMemberOrdinal);
    startConversion = startValue != nullptr
        ? classifyImplicit(startResult.typeKey, "builtin:int", startResult.integerConstant)
        : ProjectImplicitConversion{};
    endConversion = endValue != nullptr
        ? classifyImplicit(endResult.typeKey, "builtin:int", endResult.integerConstant)
        : ProjectImplicitConversion{};
    if ((startValue != nullptr && (!startResult.valid || !startConversion.exists())) ||
        (endValue != nullptr && (!endResult.valid || !endConversion.exists())))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidIndex,
            diagnostics::DiagnosticSeverity::Error,
            rangeSyntax->range(),
            "range endpoints must convert implicitly to int");
        return false;
    }
    const bool negativeStart = startValue != nullptr && !startFromEnd &&
        startResult.integerConstant.has_value() && startResult.integerConstant->isNegative;
    const bool negativeEnd = endValue != nullptr && !endFromEnd &&
        endResult.integerConstant.has_value() && endResult.integerConstant->isNegative;
    if (!negativeStart && !negativeEnd) return true;
    diagnostics.report(
        diagnostics::DiagnosticId::ConstantIndexOutOfBounds,
        diagnostics::DiagnosticSeverity::Error,
        negativeStart ? writtenStart->range() : writtenEnd->range(),
        "a range endpoint cannot be negative");
    return false;
}

void ProjectMethodBodyModel::recordRangeSliceBinding(
    const ast::AstNode* expression,
    const ExpressionResult& base,
    const ExpressionResult& sliceResult,
    std::size_t receiverType,
    const MethodCallBinding& lengthCall,
    const MethodCallBinding& sliceCall,
    const ExpressionResult& startResult,
    const ExpressionResult& endResult,
    const ProjectImplicitConversion& startConversion,
    const ProjectImplicitConversion& endConversion,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal)
{
    const ast::AstNode* rangeSyntax = expression->children().empty()
        ? expression->second() : expression->children().front();
    const ast::AstNode* writtenStart = rangeSyntax->first();
    const ast::AstNode* writtenEnd = rangeSyntax->second();
    const bool startFromEnd = writtenStart != nullptr &&
        writtenStart->kind() == ast::AstNodeKind::FromEndExpression;
    const bool endFromEnd = writtenEnd != nullptr &&
        writtenEnd->kind() == ast::AstNodeKind::FromEndExpression;
    const ast::AstNode* startValue = startFromEnd ? writtenStart->first() : writtenStart;
    const ast::AstNode* endValue = endFromEnd ? writtenEnd->first() : writtenEnd;
    MethodRangeSliceBinding binding;
    binding.syntax = expression;
    binding.methodMemberOrdinal = methodMemberOrdinal;
    binding.scopeOrdinal = scopeOrdinal;
    binding.receiverSyntax = expression->first();
    binding.receiverTypeKey = base.typeKey;
    binding.receiverTypeOrdinal = receiverType;
    binding.lengthMemberOrdinal = *lengthCall.selectedMemberOrdinal;
    binding.sliceMemberOrdinal = *sliceCall.selectedMemberOrdinal;
    binding.resultTypeKey = sliceResult.typeKey;
    binding.startSyntax = startValue;
    binding.endSyntax = endValue;
    binding.startConversion = startConversion;
    binding.endConversion = endConversion;
    binding.hasStart = writtenStart != nullptr;
    binding.startFromEnd = startFromEnd;
    binding.hasEnd = writtenEnd != nullptr;
    binding.endFromEnd = endFromEnd;
    const auto isZero = [](const ExpressionResult& endpoint)
    {
        return endpoint.integerConstant.has_value() &&
            !endpoint.integerConstant->isNegative && endpoint.integerConstant->value == 0U;
    };
    const bool provenStartZero = writtenStart == nullptr ||
        (isZero(startResult) && !startFromEnd);
    const bool provenStartLength = writtenStart != nullptr &&
        isZero(startResult) && startFromEnd;
    const bool provenEndZero = writtenEnd != nullptr && isZero(endResult) && !endFromEnd;
    const bool provenEndLength = writtenEnd == nullptr || (isZero(endResult) && endFromEnd);
    binding.boundsStaticallyProven =
        (provenStartZero && (provenEndZero || provenEndLength)) ||
        (provenStartLength && provenEndLength);
    rangeSliceBindings_.push_back(std::move(binding));
}

bool ProjectMethodBodyModel::analyzeRangeIndexer(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ExpressionResult& base,
    std::size_t receiverType,
    std::size_t accessingTypeOrdinal,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    const auto& indexSyntaxes = expression->children();
    const ast::AstNode* rangeSyntax = !indexSyntaxes.empty()
        ? indexSyntaxes.front() : expression->second();
    if (rangeSyntax == nullptr || rangeSyntax->kind() != ast::AstNodeKind::RangeExpression ||
        (!indexSyntaxes.empty() && indexSyntaxes.size() != 1)) return false;
    // NativeSharp 3.0 T01-F04. Range indexing is a statically resolved structural
    // protocol, not a hidden allocation and not a magical Range carrier:
    //
    //     receiver[start..end]
    //       -> receiver.Slice(resolvedStart, resolvedEnd - resolvedStart)
    //
    // `Length()` supplies omitted/from-end endpoints. Both methods are bound now,
    // but HIR keeps them in one node so an effectful receiver is evaluated once.
    ExpressionResult sliceResult;
    MethodCallBinding lengthCall;
    MethodCallBinding sliceCall;
    if (!resolveRangeProtocol(
            sliceResult, lengthCall, sliceCall, expression, base, receiverType,
            accessingTypeOrdinal, methodMemberOrdinal, scopeOrdinal, diagnostics)) return true;
    if (!validateRangeProtocolMembers(rangeSyntax, lengthCall, sliceCall, diagnostics)) return true;
    ExpressionResult startResult;
    ExpressionResult endResult;
    ProjectImplicitConversion startConversion;
    ProjectImplicitConversion endConversion;
    if (!validateRangeEndpoints(
            startConversion, endConversion, startResult, endResult, rangeSyntax,
            methodMemberOrdinal, diagnostics)) return true;
    recordRangeSliceBinding(
        expression, base, sliceResult, receiverType, lengthCall, sliceCall,
        startResult, endResult, startConversion, endConversion,
        methodMemberOrdinal, scopeOrdinal);
    result = sliceResult;
    result.assignable = false;
    result.addressable = false;
    return true;
}

bool ProjectMethodBodyModel::analyzeUserIndexer(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ExpressionResult& base,
    const std::vector<ExpressionResult>& indices,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    // C6 "struct members": an indexer is a pair of accessor METHODS, and a struct
    // instance method already works -- it takes the receiver's address, which is exactly
    // what an indexer on a value needs in order to be able to write through it. This used
    // to ask referenceTypeOrdinalForTypeKey, which answers only for classes and
    // interfaces, so `m[0]` on a struct fell through to the raw-pointer branch below and
    // was reported as "index expression requires an indexer reference or a pointer".
    // Matrix and vector types -- the whole reason a game framework has structs -- are
    // indexed constantly.
    const std::optional<std::size_t> receiverType = base.valid
        ? accessorReceiverTypeOrdinalForTypeKey(base.typeKey) : std::nullopt;
    if (!receiverType.has_value()) return false;
    const ClassMemberSymbol* caller = methodSymbol(methodMemberOrdinal);
    if (caller == nullptr) return true;
    const std::size_t accessingTypeOrdinal = caller->declaringTypeOrdinal;
    if (indices.size() == 1 && analyzeRangeIndexer(
            result, expression, base, *receiverType, accessingTypeOrdinal,
            methodMemberOrdinal, scopeOrdinal, diagnostics)) return true;
    const std::vector<std::size_t> getters = stage38AccessorCandidates(
        *receiverType, "get_Item", false, true, false,
        accessingTypeOrdinal, *receiverType);
    if (getters.empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidIndex,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "reference type has no accessible Stage 38 indexer getter");
        return true;
    }
    const bool interfaceCall = interfaceModel_ != nullptr &&
        interfaceModel_->isInterface(*receiverType);
    result = resolveSyntheticCall(
        expression, methodMemberOrdinal, scopeOrdinal, "get_Item", getters, indices,
        expression->first(), base.typeKey, receiverType, false, interfaceCall,
        diagnostics, true, base.readonlyReceiver);
    const std::vector<std::size_t> setters = stage38AccessorCandidates(
        *receiverType, "set_Item", false, false, false,
        accessingTypeOrdinal, *receiverType);
    const bool referenceGetter = result.addressable;
    result.assignable = result.valid && !base.readonlyReceiver &&
        (referenceGetter || !setters.empty());
    result.addressable = result.valid && !base.readonlyReceiver && referenceGetter;
    result.readonlyReceiver = base.readonlyReceiver;
    result.receiverSyntax = expression->first();
    result.receiverTypeKey = base.typeKey;
    result.receiverTypeOrdinal = receiverType;
    return true;
}

bool ProjectMethodBodyModel::validateFixedArrayIndices(
    std::string& indexedType,
    bool& fixedOnePastAddress,
    const ast::AstNode* expression,
    const ExpressionResult& base,
    const std::vector<ExpressionResult>& indices,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics) const
{
    const std::optional<std::string> fixedElementType = memberModel_ != nullptr
        ? ProjectMemberModel::fixedArrayElementTypeKey(base.typeKey) : std::nullopt;
    bool validFixedIndices = base.valid && fixedElementType.has_value() && !indices.empty();
    if (!validFixedIndices) return false;
    for (std::size_t dimensionOrdinal = 0;
         dimensionOrdinal < indices.size(); ++dimensionOrdinal)
    {
        const ExpressionResult& index = indices[dimensionOrdinal];
        const std::optional<std::string> element =
            ProjectMemberModel::fixedArrayElementTypeKey(indexedType);
        const std::optional<std::uint64_t> count =
            ProjectMemberModel::fixedArrayCount(indexedType);
        if (!index.valid || !ProjectTypeConversion::isIntegral(index.typeKey) ||
            !element.has_value() || !count.has_value())
        {
            return false;
        }
        if (index.integerConstant.has_value())
        {
            const bool exactlyOnePast = !index.integerConstant->isNegative &&
                index.integerConstant->value == *count;
            const bool admittedOnePast = exactlyOnePast && formingAddressOperand_ &&
                dimensionOrdinal + 1U == indices.size();
            if (index.integerConstant->isNegative || index.integerConstant->value > *count ||
                (exactlyOnePast && !admittedOnePast))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::ConstantIndexOutOfBounds,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->children().empty()
                        ? expression->range() : expression->children()[dimensionOrdinal]->range(),
                    "constant fixed-array index is outside [0, " +
                        std::to_string(*count) + ")");
            }
            if (admittedOnePast) fixedOnePastAddress = true;
        }
        indexedType = *element;
    }
    return true;
}

const MethodLocalSymbol* ProjectMethodBodyModel::variableArrayLocal(
    const ast::AstNode* expression,
    const ExpressionResult& base,
    [[maybe_unused]] std::size_t scopeOrdinal) const
{
    if (!base.valid || expression->first() == nullptr ||
        expression->first()->kind() != ast::AstNodeKind::IdentifierExpression) return nullptr;
    const std::optional<ScopeName> baseName = lookupScopeName(
        scopeOrdinal, expression->first()->text());
    if (!baseName.has_value() || baseName->kind != ScopeNameKind::Local ||
        baseName->semanticOrdinal >= locals_.size() ||
        !locals_[baseName->semanticOrdinal].isVariableLengthArray) return nullptr;
    return &locals_[baseName->semanticOrdinal];
}

bool ProjectMethodBodyModel::analyzeVariableArrayIndex(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const std::vector<ExpressionResult>& indices,
    const MethodLocalSymbol* variableArrayLocal,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    if (variableArrayLocal == nullptr ||
        variableArrayLocal->variableLengthArrayExtentSyntaxes.size() <= 1) return false;
    bool validIndices =
        indices.size() == variableArrayLocal->variableLengthArrayExtentSyntaxes.size();
    for (const ExpressionResult& index : indices)
    {
        validIndices = validIndices && index.valid &&
            ProjectTypeConversion::isIntegral(index.typeKey) && index.typeKey != "builtin:bool";
    }
    if (!validIndices)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidIndex,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "multidimensional VLA indexing requires one integral index per dimension");
        return true;
    }
    result.typeKey = variableArrayLocal->variableLengthArrayElementTypeKey;
    result.valid = true;
    result.assignable = true;
    result.addressable = true;
    MethodCoreBinding binding;
    binding.kind = MethodCoreOperationKind::RawElement;
    binding.syntax = expression;
    binding.methodMemberOrdinal = methodMemberOrdinal;
    binding.scopeOrdinal = scopeOrdinal;
    binding.resultTypeKey = result.typeKey;
    binding.elementTypeKey = result.typeKey;
    binding.assignable = true;
    coreBindings_.push_back(std::move(binding));
    variableArrayIndexBindings_.push_back(MethodVariableArrayIndexBinding{
        expression, variableArrayLocal->variableLengthArrayExtentLocalOrdinals});
    return true;
}

void ProjectMethodBodyModel::analyzeRawIndex(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ExpressionResult& base,
    const std::vector<ExpressionResult>& indices,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    // C2.1 "fixed arrays". Two bases share one rule here, because their address
    // arithmetic is the same and neither carries a length: a FIXED ARRAY (`float[4]`,
    // inline storage inside a struct or a local) and a RAW POINTER (`int*`). Both are the
    // language's raw tier -- no bounds check, no null check, C semantics by design, the
    // same tier `*p` and `&x` already live in -- and both are what a C header's
    // `float color[4]` and `float* data` actually mean.
    //
    // MEASURED before this round: the fixed-array TYPE parsed and laid out correctly
    // (`float[4]` at offset 0, size 16) but `c.rgba[i]` was rejected with "index
    // expression requires an indexer reference or a pointer with one integral index" --
    // and the pointer half of that very message was ALSO a lie: `p[0]` passed this check
    // and then died in HIR with "Stage 38 OOP lowering requires class/interface index
    // expressions to resolve to an indexer accessor". Neither shape had a lowering.
    const std::optional<std::string> flexibleElementType = memberModel_ != nullptr
        ? ProjectMemberModel::flexibleArrayElementTypeKey(base.typeKey) : std::nullopt;
    const bool rawPointerBase = ProjectTypeConversion::isPointer(base.typeKey);
    bool fixedOnePastAddress = false;
    std::string indexedType = base.typeKey;
    const bool validFixedIndices = validateFixedArrayIndices(
        indexedType, fixedOnePastAddress, expression, base, indices, diagnostics);
    const MethodLocalSymbol* local = variableArrayLocal(expression, base, scopeOrdinal);
    if (analyzeVariableArrayIndex(
            result, expression, indices, local,
            methodMemberOrdinal, scopeOrdinal, diagnostics)) return;
    const bool validPointerIndex = base.valid && rawPointerBase && indices.size() == 1 &&
        indices.front().valid && ProjectTypeConversion::isIntegral(indices.front().typeKey);
    const bool validFlexibleIndex = base.valid && flexibleElementType.has_value() &&
        indices.size() == 1 && indices.front().valid &&
        ProjectTypeConversion::isIntegral(indices.front().typeKey) &&
        indices.front().typeKey != "builtin:bool";
    if (validFixedIndices || validFlexibleIndex || validPointerIndex)
    {
        result.typeKey = validFixedIndices ? indexedType : validFlexibleIndex
            ? *flexibleElementType : std::string(base.typeKey.substr(0, base.typeKey.size() - 1));
        result.valid = true;
        const bool readonlyInlineElement = base.readonlyReceiver &&
            (validFixedIndices || validFlexibleIndex);
        result.assignable = !readonlyInlineElement &&
            !ProjectMemberModel::fixedArrayElementTypeKey(result.typeKey).has_value();
        result.addressable = true;
        result.readonlyReceiver = readonlyInlineElement;
        result.onePastEndAddress = fixedOnePastAddress;
        coreBindings_.push_back(MethodCoreBinding{
            MethodCoreOperationKind::RawElement, expression, methodMemberOrdinal,
            scopeOrdinal, result.typeKey, result.typeKey, result.assignable,
            std::nullopt, std::nullopt});
    }
    else if (base.valid)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidIndex,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "fixed-array indices must be integral and cannot exceed its rank; raw pointers "
            "require exactly one integral index");
    }
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeExpressionKindIndexExpression(
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
    if (expression->kind() != ast::AstNodeKind::IndexExpression) return result;
    ExpressionResult base;
    std::vector<ExpressionResult> indices;
    analyzeIndexOperands(
        base, indices, expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
    if (analyzeNativeArrayIndex(
            result, expression, base, indices,
            methodMemberOrdinal, scopeOrdinal, diagnostics)) return result;
    if (analyzeNativeStringIndex(
            result, expression, base, indices,
            methodMemberOrdinal, scopeOrdinal, diagnostics)) return result;
    if (analyzeUserIndexer(
            result, expression, base, indices,
            methodMemberOrdinal, scopeOrdinal, diagnostics)) return result;
    analyzeRawIndex(
        result, expression, base, indices, methodMemberOrdinal, scopeOrdinal, diagnostics);
    return result;
}

// CC1 fase 4A -- `sizeof`. Tres cosas cambian aqui respecto a lo que habia, y las tres
        // eran defectos reales:
        //
        // 1. TIPO DEL RESULTADO: `builtin:nuint`, no `builtin:ulong`.
        //    Aviso sobre la justificacion: el documento de origen dice que hay que unificar con
        //    "el pipeline clasico", que ya bajaria `sizeof` a una constante `nuint`. ESO NO EXISTE
        //    EN ESTE ARBOL -- barrido completo: `sizeof` solo aparece en el lexer, en el parser y
        //    aqui; no hay caso en Sema ni en el HIR clasico, ni un solo test. No hay con que
        //    comparar, asi que la razon no puede ser esa. La razon es propia: `sizeof` produce una
        //    CUENTA DE BYTES, y el tipo sin signo del ancho del puntero es lo que consumen la
        //    aritmetica de punteros y `stackalloc`; darle `ulong` obligaria a una conversion en
        //    cada uso natural y ademas mentiria en un target de 32 bits.
        //
        // 2. OPERANDO NO EVALUADO. `sizeof(expression)` mide el TIPO de su operando; el operando no
        //    se ejecuta. Antes se analizaba como codigo normal, asi que `sizeof(x = 1)` marcaba `x`
        //    como asignada, `sizeof(move(o))` movia el propietario, y un borrow dentro del operando
        //    sobrevivia. El operando se sigue analizando -- hay que resolver nombres, miembros y
        //    tipos para saber QUE se esta midiendo, y sus errores deben seguir apareciendo -- pero
        //    el estado de flujo se captura antes y se restaura despues, de modo que la propiedad
        //    "el estado de flujo antes y despues es identico" se cumple POR CONSTRUCCION y no por
        //    enumerar cada efecto que habria que suprimir. OwnershipFlowSnapshot ya lleva las tres
        //    familias que importan: moves (must y may), borrows y `assignedLocals` (definite
        //    assignment). Es la misma maquinaria que usan las ramas de `?:` y `&&`/`||`.
        //    Los bindings que el analisis del operando deje registrados quedan huerfanos y son
        //    inertes: el HIR los indexa POR NODO DE SINTAXIS y solo los consulta al bajar ese nodo,
        //    y el operando de un `sizeof` no se baja nunca -- se baja una constante.
        //
        // 3. VALIDACION. `SizeofTypeExpression` no tocaba siquiera su `typeSyntax()`: `sizeof(void)`
        //    y `sizeof(TipoQueNoExiste)` pasaban el analisis semantico sin una palabra y morian
        //    despues en el `default` del HIR con "unsupported Stage 34 OOP expression kind". Ahora
        //    se resuelve el tipo aqui y se rechaza lo que no tiene tamano, con DiagnosticId::
        //    InvalidSizeof -- que estaba declarado desde hacia tiempo y no se usaba en ningun sitio.
ProjectMethodBodyModel::ExpressionResult
ProjectMethodBodyModel::
analyzeExpressionKindSizeofTypeExpressionOrSizeofExpressionOrAlignofTypeExpressionOrAlignofExpression(
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
        case ast::AstNodeKind::SizeofTypeExpression:
        case ast::AstNodeKind::SizeofExpression:
        case ast::AstNodeKind::AlignofTypeExpression:
        case ast::AstNodeKind::AlignofExpression:
        {
            const bool alignmentQuery =
                expression->kind() == ast::AstNodeKind::AlignofTypeExpression ||
                expression->kind() == ast::AstNodeKind::AlignofExpression;
            std::string measuredTypeKey;
            if (expression->kind() == ast::AstNodeKind::SizeofExpression ||
                expression->kind() == ast::AstNodeKind::AlignofExpression)
            {
                const OwnershipFlowSnapshot beforeOperand = captureOwnershipFlow();
                const bool previousUnevaluated = inUnevaluatedOperand_;
                inUnevaluatedOperand_ = true;
                const ExpressionResult operand = analyzeExpression(
                    expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics,
                    /*allowMethodGroup=*/true);
                inUnevaluatedOperand_ = previousUnevaluated;
                restoreOwnershipFlow(beforeOperand);
                if (!operand.valid && !operand.methodGroup)
                {
                    break;
                }
                if (operand.methodGroup)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidSizeof,
                        diagnostics::DiagnosticSeverity::Error,
                        expression->range(),
                    alignmentQuery
                        ? "alignof requires a value or a type; a method group has no alignment"
                        : "sizeof requires a value or a type; a method group has no size");
                    break;
                }
                measuredTypeKey = operand.typeKey;
            }
            else
            {
                if (memberModel_ == nullptr || nameLookup_ == nullptr)
                {
                    break;
                }
                measuredTypeKey = memberModel_->canonicalTypeKey(expression->typeSyntax(), *nameLookup_);
                if (isErrorType(measuredTypeKey))
                {
                    break;
                }
            }

            if (measuredTypeKey == "builtin:void")
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidSizeof,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    alignmentQuery ? "alignof(void) has no alignment" : "sizeof(void) has no size");
                break;
            }

            MethodCoreBinding binding;
            binding.kind = alignmentQuery ? MethodCoreOperationKind::AlignofConstant
                                          : MethodCoreOperationKind::SizeofConstant;
            binding.syntax = expression;
            binding.methodMemberOrdinal = methodMemberOrdinal;
            binding.scopeOrdinal = scopeOrdinal;
            binding.resultTypeKey = "builtin:nuint";
            // El tipo MEDIDO, no el tamano. Ver el comentario de SizeofConstant.
            binding.elementTypeKey = measuredTypeKey;
            coreBindings_.push_back(std::move(binding));

            result.typeKey = "builtin:nuint";
            result.valid = true;
            break;
        }
        default:
            break;
    }
    return result;
}
}
