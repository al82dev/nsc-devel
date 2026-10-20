#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
constexpr std::uint8_t kLocalMoved = 1U << 0U;
constexpr std::uint8_t kLocalMaybeMoved = 1U << 1U;
constexpr std::uint8_t kParameterMoved = 1U << 0U;
constexpr std::uint8_t kParameterMaybeMoved = 1U << 1U;
}

void ProjectMethodBodyModel::validateLoopExpressionBackEdgeLocals(
    const ast::AstNode* expression,
    const OwnershipFlowSnapshot& loopEntry,
    const OwnershipFlowSnapshot& backEdge,
    std::size_t localCountAtBodyEntry,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    for (std::size_t localOrdinal = 0; localOrdinal < localCountAtBodyEntry; ++localOrdinal)
    {
        if (localOrdinal >= locals_.size() ||
            (!locals_[localOrdinal].isRaii && !locals_[localOrdinal].isMoveOnly)) continue;
        const bool aliveAtEntry = !snapshotLocalHasAnyFlag(
            loopEntry, localOrdinal, kLocalMoved | kLocalMaybeMoved);
        const bool movedAtBackEdge = snapshotLocalHasAnyFlag(
            backEdge, localOrdinal, kLocalMoved | kLocalMaybeMoved);
        if (!aliveAtEntry || !movedAtBackEdge) continue;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "RAII owner '" + locals_[localOrdinal].name +
                "' may remain moved across a loop-expression back-edge; "
                "reinitialize it before the next iteration");
    }
}

void ProjectMethodBodyModel::validateLoopExpressionBackEdgeParameters(
    const ast::AstNode* expression,
    const ClassMemberSymbol* loopMethod,
    const OwnershipFlowSnapshot& loopEntry,
    const OwnershipFlowSnapshot& backEdge,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (loopMethod == nullptr) return;
    for (std::size_t parameterOrdinal = 0;
         parameterOrdinal < loopMethod->parameters.size(); ++parameterOrdinal)
    {
        if (loopMethod->parameters[parameterOrdinal].ownership !=
            sema::OwnershipKind::Owned) continue;
        const bool aliveAtEntry = !snapshotParameterHasAnyFlag(
            loopEntry, parameterOrdinal, kParameterMoved | kParameterMaybeMoved);
        const bool movedAtBackEdge = snapshotParameterHasAnyFlag(
            backEdge, parameterOrdinal, kParameterMoved | kParameterMaybeMoved);
        if (!aliveAtEntry || !movedAtBackEdge) continue;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "owned parameter '" + loopMethod->parameters[parameterOrdinal].name +
                "' may remain moved across a loop-expression back-edge; "
                "transfer it only on an exiting path");
    }
}

void ProjectMethodBodyModel::validateLoopExpressionBackEdgePlaces(
    const ast::AstNode* expression,
    const OwnershipFlowSnapshot& loopEntry,
    const OwnershipFlowSnapshot& backEdge,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    std::unordered_set<std::string> entryPartial = loopEntry.partiallyMovedPlaces;
    entryPartial.insert(
        loopEntry.maybePartiallyMovedPlaces.begin(),
        loopEntry.maybePartiallyMovedPlaces.end());
    std::unordered_set<std::string> backPartial = backEdge.partiallyMovedPlaces;
    backPartial.insert(
        backEdge.maybePartiallyMovedPlaces.begin(),
        backEdge.maybePartiallyMovedPlaces.end());
    for (const std::string& place : backPartial)
    {
        if (entryPartial.contains(place)) continue;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "field path '" + place +
                "' may remain moved across a loop-expression back-edge; "
                "reinitialize it before the next iteration");
    }
}

std::optional<std::string> ProjectMethodBodyModel::loopExpressionCommonType(
    const ast::AstNode* expression,
    const LoopContext& completed,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    std::string commonType;
    for (const LoopContext::BreakValue& item : completed.breakValues)
    {
        const ExpressionResult& value = item.value;
        if (value.typeKey == "builtin:never") continue;
        if (commonType.empty())
        {
            commonType = value.typeKey;
            continue;
        }
        if (value.typeKey == commonType) continue;
        const std::optional<std::string> numeric = ProjectTypeConversion::commonNumericType(
            commonType, value.typeKey, std::nullopt, value.integerConstant);
        if (numeric.has_value())
        {
            commonType = *numeric;
            continue;
        }
        if (classifyImplicit(value.typeKey, commonType, value.integerConstant).exists()) continue;
        if (classifyImplicit(commonType, value.typeKey).exists())
        {
            commonType = value.typeKey;
            continue;
        }
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            item.syntax != nullptr ? item.syntax->range() : expression->range(),
            "loop expression break values have no common type between '" + commonType +
                "' and '" + value.typeKey + "'");
        return std::nullopt;
    }
    if (commonType.empty()) commonType = "builtin:never";
    return commonType;
}

void ProjectMethodBodyModel::recordLoopExpressionBindings(
    const ast::AstNode* expression,
    const LoopContext& completed,
    std::string_view commonType,
    std::size_t methodMemberOrdinal,
    std::size_t loopScope)
{
    MethodLoopExpressionBinding loopBinding;
    loopBinding.syntax = expression;
    loopBinding.methodMemberOrdinal = methodMemberOrdinal;
    loopBinding.scopeOrdinal = loopScope;
    loopBinding.label = expression->text();
    loopBinding.resultTypeKey = commonType;
    loopExpressionBindingBySyntax_[expression] = loopExpressionBindings_.size();
    loopExpressionBindings_.push_back(std::move(loopBinding));
    for (const LoopContext::BreakValue& item : completed.breakValues)
    {
        MethodLoopBreakBinding breakBinding;
        breakBinding.syntax = item.syntax;
        breakBinding.targetLoopSyntax = expression;
        breakBinding.resultTypeKey = commonType;
        if (item.value.typeKey != "builtin:never")
        {
            breakBinding.conversion = classifyImplicit(
                item.value.typeKey, commonType, item.value.integerConstant);
        }
        loopBreakBindingBySyntax_[item.syntax] = loopBreakBindings_.size();
        loopBreakBindings_.push_back(std::move(breakBinding));
    }
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::loopExpressionResult(
    const LoopContext& completed,
    std::string commonType) const
{
    ExpressionResult result;
    result.typeKey = std::move(commonType);
    result.valid = true;
    if (completed.breakValues.empty()) return result;
    const auto firstValue = std::ranges::find_if(
        completed.breakValues, [](const LoopContext::BreakValue& item)
        {
            return item.value.typeKey != "builtin:never";
        });
    if (firstValue == completed.breakValues.end()) return result;
    result.ownership = firstValue->value.ownership;
    result.ownershipTransfer = std::ranges::all_of(
        completed.breakValues, [&](const LoopContext::BreakValue& item)
        {
            return item.value.typeKey == "builtin:never" ||
                (item.value.ownership == result.ownership &&
                 item.value.ownershipTransfer);
        });
    result.ownershipFreshOwner = std::ranges::all_of(
        completed.breakValues, [](const LoopContext::BreakValue& item)
        {
            return item.value.typeKey == "builtin:never" ||
                item.value.ownershipFreshOwner;
        });
    return result;
}

bool ProjectMethodBodyModel::validateLoopExpressionSyntax(
    const ast::AstNode* expression,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (expression == nullptr || expression->first() == nullptr ||
        expression->first()->kind() != ast::AstNodeKind::BlockStatement)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression != nullptr ? expression->range() : SourceRange(),
            "a loop expression requires a block body");
        return false;
    }
    if (expression->text().empty() || !std::ranges::any_of(
            loopStack_, [&](const LoopContext& active)
            {
                return active.isExpression && active.label == expression->text();
            })) return true;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "nested loop label '" + expression->text() + "' is already active");
    return false;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeLoopExpression(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    if (!validateLoopExpressionSyntax(expression, diagnostics)) return result;

    const std::size_t loopScope = createScope(
        MethodScopeKind::Block, methodMemberOrdinal, scopeOrdinal);
    const OwnershipFlowSnapshot loopEntry = captureOwnershipFlow();
    const std::size_t localCountAtBodyEntry = locals_.size();
    const ClassMemberSymbol* loopMethod = methodSymbol(methodMemberOrdinal);
    LoopContext context;
    context.targetScopeOrdinal = loopScope;
    context.breakCleanupDepth = activeCleanupRegions_.size();
    context.continueCleanupDepth = activeCleanupRegions_.size();
    context.syntax = expression;
    context.label = expression->text();
    context.isExpression = true;
    loopStack_.push_back(std::move(context));
    analyzeBlock(expression->first(), methodMemberOrdinal, loopScope, true, diagnostics);
    const OwnershipFlowSnapshot fallthroughState = captureOwnershipFlow();
    LoopContext completed = std::move(loopStack_.back());
    loopStack_.pop_back();

    std::vector<OwnershipFlowSnapshot> backEdges = std::move(completed.continueStates);
    if (fallthroughState.reachable) backEdges.push_back(fallthroughState);
    OwnershipFlowSnapshot noBackEdge;
    noBackEdge.reachable = false;
    const OwnershipFlowSnapshot backEdge = joinOwnershipFlows(noBackEdge, backEdges);
    if (backEdge.reachable)
    {
        validateLoopExpressionBackEdgeLocals(
            expression, loopEntry, backEdge, localCountAtBodyEntry, diagnostics);
        validateLoopExpressionBackEdgeParameters(
            expression, loopMethod, loopEntry, backEdge, diagnostics);
        validateLoopExpressionBackEdgePlaces(
            expression, loopEntry, backEdge, diagnostics);
    }
    OwnershipFlowSnapshot postLoop;
    postLoop.reachable = false;
    postLoop = joinOwnershipFlows(postLoop, completed.breakStates);
    restoreOwnershipFlow(postLoop);
    if (std::ranges::any_of(completed.breakValues, [](const LoopContext::BreakValue& item)
        {
            return !item.value.valid;
        })) return result;

    const std::optional<std::string> commonType =
        loopExpressionCommonType(expression, completed, diagnostics);
    if (!commonType.has_value()) return result;
    recordLoopExpressionBindings(
        expression, completed, *commonType, methodMemberOrdinal, loopScope);
    return loopExpressionResult(completed, *commonType);
}
}
