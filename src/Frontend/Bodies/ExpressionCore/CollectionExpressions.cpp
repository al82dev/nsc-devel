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

std::vector<ProjectMethodBodyModel::ExpressionResult>
ProjectMethodBodyModel::analyzeNullConditionalIndices(
    const ast::AstNode* expression,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    std::vector<ExpressionResult> indices;
    const auto& indexSyntaxes = expression->children();
    if (!indexSyntaxes.empty())
    {
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
    return indices;
}

bool ProjectMethodBodyModel::analyzeNullConditionalNativeArray(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ExpressionResult& base,
    std::span<const ExpressionResult> indices,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    if (!base.valid || !ProjectTypeConversion::isNativeArray(base.typeKey)) return false;
    // NS304-T10-F05: sólo el array nativo tiene READ null-conditional (receptor + índice);
    // el element-load condicional se marca como NullConditionalArrayElement.
    const std::size_t arrayRank = ProjectTypeConversion::nativeArrayRank(base.typeKey);
    bool validIndices = indices.size() == arrayRank;
    for (const ExpressionResult& index : indices)
    {
        validIndices = validIndices && index.valid &&
            ProjectTypeConversion::isIntegral(index.typeKey) &&
            index.typeKey != "builtin:bool";
    }
    if (validIndices)
    {
        const std::optional<std::string> element =
            ProjectTypeConversion::nativeArrayElementType(base.typeKey);
        if (element.has_value())
        {
            result.typeKey = *element;
            result.valid = true;
            result.assignable = false;
            coreBindings_.push_back(MethodCoreBinding{
                MethodCoreOperationKind::NullConditionalArrayElement,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                result.typeKey,
                result.typeKey,
                true, std::nullopt, std::nullopt});
        }
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

bool ProjectMethodBodyModel::analyzeNullConditionalUserIndex(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ExpressionResult& base,
    const std::vector<ExpressionResult>& indices,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    // NS304-T10-F05: a USER indexer (`this[...]`) supports the null-conditional form: the
    // receiver is null-checked and get_Item runs only on the non-null path.
    const std::optional<std::size_t> receiverType = base.valid
        ? accessorReceiverTypeOrdinalForTypeKey(base.typeKey) : std::nullopt;
    if (!receiverType.has_value() || indices.size() != 1 || !indices.front().valid)
        return false;
    const ClassMemberSymbol* caller = methodSymbol(methodMemberOrdinal);
    if (caller == nullptr) return false;
    const std::size_t accessingTypeOrdinal = caller->declaringTypeOrdinal;
    const std::vector<std::size_t> getters = stage38AccessorCandidates(
        *receiverType, "get_Item", false, true, false,
        accessingTypeOrdinal, *receiverType);
    if (getters.empty()) return false;
    const bool interfaceCall =
        interfaceModel_ != nullptr && interfaceModel_->isInterface(*receiverType);
    result = resolveSyntheticCall(
        expression, methodMemberOrdinal, scopeOrdinal, "get_Item", getters,
        indices, expression->first(), base.typeKey, receiverType, false,
        interfaceCall, diagnostics, true, base.readonlyReceiver);
    const std::vector<std::size_t> setters = stage38AccessorCandidates(
        *receiverType, "set_Item", false, false, false,
        accessingTypeOrdinal, *receiverType);
    // EN: Freeze 3.0 P3: a `ref readonly` getter is not assignable.
    // ES: Congelamiento 3.0 P3: un getter `ref readonly` no es asignable.
    const bool referenceGetter = result.addressable && !result.readonlyReceiver;
    result.assignable = result.valid && !base.readonlyReceiver &&
        (referenceGetter || !setters.empty());
    result.addressable = false;
    result.receiverSyntax = expression->first();
    result.receiverTypeKey = base.typeKey;
    result.receiverTypeOrdinal = receiverType;
    if (result.valid)
    {
        coreBindings_.push_back(MethodCoreBinding{
            MethodCoreOperationKind::NullConditionalUserIndex,
            expression, methodMemberOrdinal, scopeOrdinal,
            result.typeKey, result.typeKey,
            true, std::nullopt, std::nullopt});
    }
    return true;
}

ProjectMethodBodyModel::ExpressionResult
ProjectMethodBodyModel::analyzeExpressionKindNullConditionalIndexExpression(
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
    const ExpressionResult base = analyzeExpression(
        expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
    const std::vector<ExpressionResult> indices = analyzeNullConditionalIndices(
        expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
    if (analyzeNullConditionalNativeArray(
            result, expression, base, indices,
            methodMemberOrdinal, scopeOrdinal, diagnostics))
    {
        return result;
    }
    if (analyzeNullConditionalUserIndex(
            result, expression, base, indices,
            methodMemberOrdinal, scopeOrdinal, diagnostics))
    {
        return result;
    }
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "null-conditional index access ('obj?[i]') is only supported on native arrays");
    result.valid = false;
    return result;
}

std::optional<std::size_t> ProjectMethodBodyModel::collectionBuilderMember(
    std::size_t typeOrdinal,
    std::string_view name,
    bool wantStatic) const
{
    for (const std::size_t ordinal : memberModel_->findMembers(typeOrdinal, name))
    {
        if (ordinal >= memberModel_->members().size()) continue;
        if (memberModel_->members()[ordinal].isStatic != wantStatic) continue;
        return ordinal;
    }
    return std::nullopt;
}

bool ProjectMethodBodyModel::bindCollectionBuilder(
    ExpressionResult& result,
    const ast::AstNode* expression,
    std::string_view explicitTarget,
    std::size_t typeOrdinal,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    const std::optional<std::size_t> createBuilder =
        collectionBuilderMember(typeOrdinal, "CreateBuilder", true);
    const std::optional<std::size_t> add =
        collectionBuilderMember(typeOrdinal, "Add", false);
    const std::optional<std::size_t> toCollection =
        collectionBuilderMember(typeOrdinal, "ToCollection", false);
    if (!createBuilder.has_value() || !add.has_value() || !toCollection.has_value())
        return false;
    // NS304-T09-F05: validate the builder shape and each element against the
    // Add parameter, so the HIR lowers the elements with their real types.
    const ClassMemberSymbol& addMember = memberModel_->members()[*add];
    bool elementsValid = addMember.parameters.size() == 1U;
    if (!elementsValid)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "a collection builder Add must take exactly one parameter");
    }
    if (elementsValid)
    {
        for (const ast::AstNode* child : expression->children())
        {
            if (child == nullptr) continue;
            ExpressionResult elementValue = analyzeExpression(
                child, methodMemberOrdinal, scopeOrdinal, diagnostics,
                false, false, true);
            if (!elementValue.valid ||
                !classifyImplicit(
                    elementValue.typeKey, addMember.parameters[0].typeKey,
                    elementValue.integerConstant).exists())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    child->range(),
                    "collection expression element is not implicitly convertible to '" +
                        addMember.parameters[0].typeKey + "'");
                elementsValid = false;
            }
        }
    }
    result.typeKey = explicitTarget;
    result.valid = elementsValid;
    if (!result.valid) return true;
    MethodCoreBinding binding;
    binding.kind = MethodCoreOperationKind::CollectionBuilder;
    binding.syntax = expression;
    binding.methodMemberOrdinal = methodMemberOrdinal;
    binding.scopeOrdinal = scopeOrdinal;
    binding.resultTypeKey = explicitTarget;
    binding.builderMemberOrdinals = {*createBuilder, *add, *toCollection};
    coreBindings_.push_back(std::move(binding));
    return true;
}

bool ProjectMethodBodyModel::collectCollectionSpreadSegments(
    ExpressionResult& result,
    const ast::AstNode* expression,
    std::vector<const ast::AstNode*>& syntaxes,
    std::vector<bool>& spreads,
    std::vector<ExpressionResult>& values,
    std::vector<std::string>& elementTypes,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    for (const ast::AstNode* child : expression->children())
    {
        const bool spread =
            child != nullptr && child->kind() == ast::AstNodeKind::SpreadElementExpression;
        ExpressionResult value;
        std::string elementType;
        if (spread)
        {
            if (child->first() == nullptr)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    child->range(),
                    "a collection expression spread requires a source expression");
                result.valid = false;
                return false;
            }
            value = analyzeExpression(
                child->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (!value.valid || !ProjectTypeConversion::isNativeArray(value.typeKey))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    child->range(),
                    "a collection expression spread requires a native array source");
                result.valid = false;
                return false;
            }
            const std::optional<std::string> element =
                ProjectTypeConversion::nativeArrayElementType(value.typeKey);
            if (!element.has_value())
            {
                result.valid = false;
                return false;
            }
            elementType = *element;
        }
        else if (child != nullptr)
        {
            value = analyzeExpression(
                child, methodMemberOrdinal, scopeOrdinal, diagnostics,
                false, false, true);
        }
        syntaxes.push_back(child);
        spreads.push_back(spread);
        values.push_back(std::move(value));
        elementTypes.push_back(std::move(elementType));
    }
    return true;
}

bool ProjectMethodBodyModel::validateCollectionSpreadSegments(
    const ast::AstNode* expression,
    std::span<const ast::AstNode* const> syntaxes,
    const std::vector<bool>& spreads,
    std::span<const ExpressionResult> values,
    std::span<const std::string> elementTypes,
    std::string_view element,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics) const
{
    bool elementsValid = true;
    for (std::size_t index = 0; index < spreads.size(); ++index)
    {
        if (spreads[index])
        {
            if (elementTypes[index] == element) continue;
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                syntaxes[index]->range(),
                "collection expression spread element type '" + elementTypes[index] +
                    "' must exactly match target element type '" + std::string(element) + "'");
            elementsValid = false;
            continue;
        }
        if (!values[index].valid || !classifyImplicit(
                values[index].typeKey, element, values[index].integerConstant).exists())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                syntaxes[index] != nullptr ? syntaxes[index]->range() : expression->range(),
                "collection expression element is not implicitly convertible to '" +
                    std::string(element) + "'");
            elementsValid = false;
        }
    }
    return elementsValid;
}

void ProjectMethodBodyModel::completeCollectionArrayResult(
    ExpressionResult& result,
    const ast::AstNode* expression,
    std::string_view element,
    bool elementsValid,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal)
{
    result.typeKey = std::string(element) + "[]";
    result.valid = elementsValid;
    if (result.valid) result.storageOwnership = sema::OwnershipKind::Owned;
    if (result.valid)
    {
        coreBindings_.push_back(MethodCoreBinding{
            MethodCoreOperationKind::ArrayAllocation,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            result.typeKey,
            std::string(element),
            false, std::nullopt, std::nullopt});
    }
}

bool ProjectMethodBodyModel::analyzeCollectionSpreadSegments(
    ExpressionResult& result,
    const ast::AstNode* expression,
    std::string_view explicitTarget,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    const bool hasSpread = std::ranges::any_of(
        expression->children(),
        [](const ast::AstNode* child)
        {
            return child != nullptr &&
                child->kind() == ast::AstNodeKind::SpreadElementExpression;
        });
    if (!hasSpread) return false;
    std::vector<const ast::AstNode*> syntaxes;
    std::vector<bool> spreads;
    std::vector<ExpressionResult> values;
    std::vector<std::string> elementTypes;
    syntaxes.reserve(expression->children().size());
    spreads.reserve(expression->children().size());
    values.reserve(expression->children().size());
    elementTypes.reserve(expression->children().size());
    if (!collectCollectionSpreadSegments(
            result, expression, syntaxes, spreads, values, elementTypes,
            methodMemberOrdinal, scopeOrdinal, diagnostics))
    {
        return true;
    }
    std::optional<std::string> element;
    if (ProjectTypeConversion::isNativeArray(explicitTarget))
        element = ProjectTypeConversion::nativeArrayElementType(explicitTarget);
    else
    {
        for (std::size_t index = 0; index < spreads.size(); ++index)
        {
            if (spreads[index])
            {
                element = elementTypes[index];
                break;
            }
        }
    }
    if (!element.has_value() || *element == "builtin:void")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "a collection expression spread requires a native array target or element type");
        result.valid = false;
        return true;
    }
    const bool elementsValid = validateCollectionSpreadSegments(
        expression, syntaxes, spreads, values, elementTypes, *element, diagnostics);
    completeCollectionArrayResult(
        result, expression, *element, elementsValid, methodMemberOrdinal, scopeOrdinal);
    return true;
}

void ProjectMethodBodyModel::analyzeCollectionElements(
    ExpressionResult& result,
    const ast::AstNode* expression,
    std::string_view explicitTarget,
    [[maybe_unused]] std::size_t methodMemberOrdinal,
    [[maybe_unused]] std::size_t scopeOrdinal,
    [[maybe_unused]] diagnostics::DiagnosticEngine& diagnostics)
{
    std::vector<ExpressionResult> elementValues;
    std::vector<const ast::AstNode*> elementSyntaxes;
    elementValues.reserve(expression->children().size());
    elementSyntaxes.reserve(expression->children().size());
    for (const ast::AstNode* initializer : expression->children())
    {
        ExpressionResult value = analyzeExpression(
            initializer, methodMemberOrdinal, scopeOrdinal, diagnostics,
            false, false, true);
        if (value.valid)
        {
            elementValues.push_back(std::move(value));
            elementSyntaxes.push_back(initializer);
        }
    }
    std::optional<std::string> element;
    if (ProjectTypeConversion::isNativeArray(explicitTarget))
        element = ProjectTypeConversion::nativeArrayElementType(explicitTarget);
    else if (!elementValues.empty())
    {
        // NS304-T09-F05: inferencia abierta — tipo numérico común de los elementos.
        element = elementValues.front().typeKey;
        for (std::size_t index = 1; index < elementValues.size() && element.has_value(); ++index)
        {
            element = ProjectTypeConversion::commonNumericType(
                *element,
                elementValues[index].typeKey,
                elementValues[index - 1].integerConstant,
                elementValues[index].integerConstant);
        }
    }
    if (!element.has_value() || *element == "builtin:void")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "a collection expression requires a native array target type or a non-empty "
            "homogeneous element list");
        result.valid = false;
        return;
    }
    bool elementsValid = elementValues.size() == expression->children().size();
    for (std::size_t index = 0; index < elementValues.size(); ++index)
    {
        const ExpressionResult& value = elementValues[index];
        if (!classifyImplicit(value.typeKey, *element, value.integerConstant).exists())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                elementSyntaxes[index]->range(),
                "collection expression element is not implicitly convertible to '" +
                    *element + "'");
            elementsValid = false;
        }
    }
    result.typeKey = *element + "[]";
    result.valid = elementsValid;
    if (result.valid) result.storageOwnership = sema::OwnershipKind::Owned;
    if (result.valid)
    {
        coreBindings_.push_back(MethodCoreBinding{
            MethodCoreOperationKind::ArrayAllocation,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            result.typeKey,
            *element,
            false, std::nullopt, std::nullopt});
    }
}

ProjectMethodBodyModel::ExpressionResult
ProjectMethodBodyModel::analyzeExpressionKindCollectionExpression(
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
    // NS304-T09-F05: `[a, b, c]` targets a native array or infers one from the
    // first homogeneous element. Every spread is materialized into fresh storage.
    const std::string explicitTarget = activeTargetTypeKey_;
    // NS304-T09-F05: a collection expression targeting a builder type lowers to
    // CreateBuilder -> Add(elements) -> ToCollection (no heap as fallback).
    if (!explicitTarget.empty() && !ProjectTypeConversion::isNativeArray(explicitTarget) &&
        inheritanceModel_ != nullptr && memberModel_ != nullptr)
    {
        const std::optional<std::size_t> typeOrdinal =
            inheritanceModel_->typeOrdinalForTypeKey(explicitTarget);
        if (typeOrdinal.has_value() && bindCollectionBuilder(
                result, expression, explicitTarget, *typeOrdinal,
                methodMemberOrdinal, scopeOrdinal, diagnostics))
        {
            return result;
        }
    }
    // NS304-T09-F05: a materialized spread contributes its native-array element type.
    if (analyzeCollectionSpreadSegments(
            result, expression, explicitTarget,
            methodMemberOrdinal, scopeOrdinal, diagnostics))
    {
        return result;
    }
    analyzeCollectionElements(
        result, expression, explicitTarget,
        methodMemberOrdinal, scopeOrdinal, diagnostics);
    return result;
}

}
