#include "nativesharp/Frontend/ProjectCallableResultMerger.h"

#include "nativesharp/Frontend/ProjectCallableAnalysisResult.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include <algorithm>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
void addBase(std::size_t& ordinal, std::size_t base) noexcept
{
    ordinal += base;
}

void addBase(std::optional<std::size_t>& ordinal, std::size_t base) noexcept
{
    if (ordinal.has_value()) *ordinal += base;
}

void addBase(std::vector<std::size_t>& ordinals, std::size_t base) noexcept
{
    for (std::size_t& ordinal : ordinals) ordinal += base;
}

void remapEscapeOrigin(
    MethodEscapeOrigin& origin,
    std::size_t scopeBase,
    std::size_t localBase) noexcept
{
    addBase(origin.maximumValidScopeOrdinal, scopeBase);
    if (origin.kind == MethodEscapeOriginKind::CurrentFrameLocal ||
        origin.kind == MethodEscapeOriginKind::VariableLengthArray)
        addBase(origin.ordinal, localBase);
}

template<typename Item>
void remapCommon(
    Item& item,
    std::size_t scopeBase,
    std::size_t localBase,
    std::size_t cleanupBase,
    std::size_t lambdaBase,
    std::size_t localBorrowBase)
{
    if constexpr (requires { item.scopeOrdinal; }) addBase(item.scopeOrdinal, scopeBase);
    if constexpr (requires { item.parentScopeOrdinal; }) addBase(item.parentScopeOrdinal, scopeBase);
    if constexpr (requires { item.ownerScopeOrdinal; }) addBase(item.ownerScopeOrdinal, scopeBase);
    if constexpr (requires { item.creationScopeOrdinal; }) addBase(item.creationScopeOrdinal, scopeBase);
    if constexpr (requires { item.maximumValidScopeOrdinal; })
        addBase(item.maximumValidScopeOrdinal, scopeBase);
    if constexpr (requires { item.sourceScopeOrdinal; }) addBase(item.sourceScopeOrdinal, scopeBase);
    if constexpr (requires { item.targetScopeOrdinal; }) addBase(item.targetScopeOrdinal, scopeBase);
    if constexpr (requires { item.referenceLifetimeScopeOrdinal; })
        addBase(item.referenceLifetimeScopeOrdinal, scopeBase);

    if constexpr (requires { item.localOrdinal; }) addBase(item.localOrdinal, localBase);
    if constexpr (requires { item.localOrdinals; }) addBase(item.localOrdinals, localBase);
    if constexpr (requires { item.sourceLocalOrdinal; }) addBase(item.sourceLocalOrdinal, localBase);
    if constexpr (requires { item.sourceOwnerLocalOrdinal; })
        addBase(item.sourceOwnerLocalOrdinal, localBase);
    if constexpr (requires { item.partialSourceOwnerLocalOrdinal; })
        addBase(item.partialSourceOwnerLocalOrdinal, localBase);
    if constexpr (requires { item.resourceLocalOrdinal; }) addBase(item.resourceLocalOrdinal, localBase);
    if constexpr (requires { item.receiverSourceLocalOrdinal; })
        addBase(item.receiverSourceLocalOrdinal, localBase);
    if constexpr (requires { item.ownershipSourceLocalOrdinal; })
        addBase(item.ownershipSourceLocalOrdinal, localBase);
    if constexpr (requires { item.moveSourceLocalOrdinal; })
        addBase(item.moveSourceLocalOrdinal, localBase);
    if constexpr (requires { item.variableLengthArrayExtentLocalOrdinals; })
        addBase(item.variableLengthArrayExtentLocalOrdinals, localBase);
    if constexpr (requires { item.variableLengthArrayRootLocalOrdinal; })
        addBase(item.variableLengthArrayRootLocalOrdinal, localBase);
    if constexpr (requires { item.referenceReborrowSourceLocalOrdinal; })
        addBase(item.referenceReborrowSourceLocalOrdinal, localBase);
    if constexpr (requires { item.extentLocalOrdinals; }) addBase(item.extentLocalOrdinals, localBase);
    if constexpr (requires { item.possibleSourceOwnerLocalOrdinals; })
        addBase(item.possibleSourceOwnerLocalOrdinals, localBase);

    if constexpr (requires { item.parentCleanupRegionOrdinal; })
        addBase(item.parentCleanupRegionOrdinal, cleanupBase);
    if constexpr (requires { item.cleanupRegionOrdinal; })
        addBase(item.cleanupRegionOrdinal, cleanupBase);
    if constexpr (requires { item.nextCleanupRegionOrdinal; })
        addBase(item.nextCleanupRegionOrdinal, cleanupBase);
    if constexpr (requires { item.cleanupRegionOrdinals; })
        addBase(item.cleanupRegionOrdinals, cleanupBase);

    if constexpr (requires { item.lambdaOrdinal; }) addBase(item.lambdaOrdinal, lambdaBase);
    if constexpr (requires { item.parentLambdaOrdinal; })
        addBase(item.parentLambdaOrdinal, lambdaBase);
    if constexpr (requires { item.localBorrowBindingOrdinal; })
        addBase(item.localBorrowBindingOrdinal, localBorrowBase);
}

void remapLocal(
    MethodLocalSymbol& local,
    std::size_t scopeBase,
    std::size_t localBase,
    std::size_t lambdaBase,
    std::size_t localBorrowBase)
{
    remapCommon(local, scopeBase, localBase, 0, lambdaBase, localBorrowBase);
    remapEscapeOrigin(local.referenceEscapeOrigin, scopeBase, localBase);
}

void remapLambdaCapture(MethodLambdaCapture& capture, std::size_t localBase) noexcept
{
    addBase(capture.localOrdinal, localBase);
    if (capture.sourceKind == MethodLambdaCaptureSourceKind::OriginalLocal ||
        capture.sourceKind == MethodLambdaCaptureSourceKind::ParentLocal)
        addBase(capture.sourceOrdinal, localBase);
}

template<typename Item>
void remapNested(
    Item& item,
    std::size_t scopeBase,
    std::size_t localBase,
    std::size_t cleanupBase,
    std::size_t lambdaBase,
    std::size_t localBorrowBase)
{
    remapCommon(item, scopeBase, localBase, cleanupBase, lambdaBase, localBorrowBase);
    if constexpr (std::is_same_v<Item, MethodLocalSymbol>)
        remapLocal(item, scopeBase, localBase, lambdaBase, localBorrowBase);
    if constexpr (std::is_same_v<Item, ProjectConstructionSite>)
        for (MethodExpressionTypeBinding& argument : item.argumentTypes)
            remapCommon(argument, scopeBase, localBase, cleanupBase, lambdaBase, localBorrowBase);
    if constexpr (std::is_same_v<Item, MethodAggregateInitializerBinding>)
        for (MethodAggregateInitializerEntryBinding& entry : item.entries)
            remapCommon(entry, scopeBase, localBase, cleanupBase, lambdaBase, localBorrowBase);
    if constexpr (std::is_same_v<Item, MethodLambdaBinding>)
        for (MethodLambdaCapture& capture : item.captures)
            remapLambdaCapture(capture, localBase);
}

template<typename Item>
void appendRemapped(
    std::vector<Item>& destination,
    std::vector<Item>& source,
    std::size_t scopeBase,
    std::size_t localBase,
    std::size_t cleanupBase,
    std::size_t lambdaBase,
    std::size_t localBorrowBase)
{
    destination.reserve(destination.size() + source.size());
    for (Item& item : source)
    {
        remapNested(item, scopeBase, localBase, cleanupBase, lambdaBase, localBorrowBase);
        destination.push_back(std::move(item));
    }
}

void mergeOwnershipStatistics(
    ProjectOwnershipFlowStatistics& destination,
    const ProjectOwnershipFlowStatistics& source) noexcept
{
    destination.snapshotsCaptured += source.snapshotsCaptured;
    destination.snapshotsRestored += source.snapshotsRestored;
    destination.joinsComputed += source.joinsComputed;
    destination.denseLocalSlotsCaptured += source.denseLocalSlotsCaptured;
    destination.denseStorageSlotsCaptured += source.denseStorageSlotsCaptured;
    destination.partialPlaceEntriesCopied += source.partialPlaceEntriesCopied;
    destination.legacyOrdinalHashEntriesCopied += source.legacyOrdinalHashEntriesCopied;
}

void mergeEscapeStatistics(
    ProjectEscapePropagationStatistics& destination,
    const ProjectEscapePropagationStatistics& source) noexcept
{
    destination.argumentOriginsClassified += source.argumentOriginsClassified;
    destination.referenceOriginEvaluations += source.referenceOriginEvaluations;
    destination.referenceContinuationFrames += source.referenceContinuationFrames;
    destination.maximumReferenceContinuationDepth = std::max(
        destination.maximumReferenceContinuationDepth,
        source.maximumReferenceContinuationDepth);
    destination.legacyGlobalCallRescans += source.legacyGlobalCallRescans;
    destination.recursiveVariableArrayFallbacks += source.recursiveVariableArrayFallbacks;
    destination.recursiveReferenceOriginFallbacks += source.recursiveReferenceOriginFallbacks;
}
}

void ProjectCallableResultMerger::merge(
    ProjectMethodBodyModel& destination,
    ProjectCallableAnalysisResult&& result)
{
    ProjectMethodBodyModel& source = result.shard();
    const std::size_t scopeBase = destination.scopes_.size();
    const std::size_t localBase = destination.locals_.size();
    const std::size_t cleanupBase = destination.cleanupRegions_.size();
    const std::size_t lambdaBase = destination.nextLambdaOrdinal_;
    const std::size_t localBorrowBase = destination.localBorrowBindings_.size();
    const std::size_t nameBase = destination.nameBindings_.size();
    const std::size_t expressionBase = destination.expressionTypes_.size();
    const std::size_t constructionBase = destination.constructionSites_.size();
    const std::size_t aggregateBase = destination.aggregateInitializerBindings_.size();
    const std::size_t deconstructionBase = destination.deconstructionBindings_.size();
    const std::size_t matchBase = destination.matchExpressionBindings_.size();
    const std::size_t loopExpressionBase = destination.loopExpressionBindings_.size();
    const std::size_t loopBreakBase = destination.loopBreakBindings_.size();
    const std::size_t callBase = destination.calls_.size();
    const std::size_t namespaceCallBase = destination.namespaceFunctionCalls_.size();

    destination.scopeNames_.reserve(
        destination.scopeNames_.size() + source.scopeNames_.size());
    for (auto& names : source.scopeNames_)
    {
        for (auto& name : names)
        {
            if (name.kind == ProjectMethodBodyModel::ScopeNameKind::Local)
                name.semanticOrdinal += localBase;
        }
        destination.scopeNames_.push_back(std::move(names));
    }

#define NSC_APPEND(field) appendRemapped(destination.field, source.field, scopeBase, localBase, \
    cleanupBase, lambdaBase, localBorrowBase)
    NSC_APPEND(scopes_);
    NSC_APPEND(locals_);
    NSC_APPEND(nameBindings_);
    NSC_APPEND(shadowingEvents_);
    NSC_APPEND(expressionTypes_);
    NSC_APPEND(constructionSites_);
    NSC_APPEND(aggregateInitializerBindings_);
    NSC_APPEND(deconstructionBindings_);
    NSC_APPEND(matchExpressionBindings_);
    NSC_APPEND(loopExpressionBindings_);
    NSC_APPEND(loopBreakBindings_);
    NSC_APPEND(runtimeTypeBindings_);
    NSC_APPEND(coreBindings_);
    NSC_APPEND(variableArrayIndexBindings_);
    NSC_APPEND(exceptionBindings_);
    NSC_APPEND(cleanupRegions_);
    NSC_APPEND(usingBindings_);
    NSC_APPEND(moveBindings_);
    NSC_APPEND(ownershipTransferBindings_);
    NSC_APPEND(borrowBindings_);
    NSC_APPEND(localBorrowBindings_);
    NSC_APPEND(borrowedParameterReassignmentBindings_);
    NSC_APPEND(switchBindings_);
    NSC_APPEND(structuredExits_);
    NSC_APPEND(cleanupContinuations_);
    NSC_APPEND(lambdaBindings_);
    NSC_APPEND(lambdaValueBindings_);
    NSC_APPEND(delegateBindings_);
    NSC_APPEND(eventBindings_);
    NSC_APPEND(optionalCarrierBindings_);
    NSC_APPEND(resultPropagationBindings_);
    NSC_APPEND(rangeSliceBindings_);
    NSC_APPEND(calls_);
    NSC_APPEND(namespaceFunctionCalls_);
#undef NSC_APPEND

    for (auto& [syntax, value] : source.staticInterpolationValues_)
        destination.staticInterpolationValues_.insert_or_assign(syntax, std::move(value));
    destination.assignedOutParameters_.insert(
        source.assignedOutParameters_.begin(), source.assignedOutParameters_.end());
    destination.analyzedCallableMembers_.insert(result.callableOrdinal());
    if (source.isolatedCallableOrdinal_.has_value() &&
        *source.isolatedCallableOrdinal_ == result.callableOrdinal() &&
        result.callableOrdinal() < destination.parameterEscapeSummaries_.size())
    {
        destination.parameterEscapeSummaries_[result.callableOrdinal()] =
            std::move(source.isolatedParameterEscapeSummary_);
    }
    ++destination.callableAnalysisStatistics_.resultsMerged;
    destination.nextLambdaOrdinal_ += source.nextLambdaOrdinal_;
    mergeOwnershipStatistics(
        destination.ownershipFlowStatistics_, source.ownershipFlowStatistics_);
    mergeEscapeStatistics(
        destination.escapePropagationStatistics_, source.escapePropagationStatistics_);
    for (std::size_t index = nameBase; index < destination.nameBindings_.size(); ++index)
    {
        const MethodNameBinding& binding = destination.nameBindings_[index];
        destination.nameBindingIndex_.record(binding.methodMemberOrdinal, binding.syntax, index);
    }
    for (std::size_t index = expressionBase; index < destination.expressionTypes_.size(); ++index)
    {
        const MethodExpressionTypeBinding& binding = destination.expressionTypes_[index];
        destination.expressionTypeBindingIndex_.record(
            binding.methodMemberOrdinal, binding.syntax, index);
    }
    for (std::size_t index = constructionBase;
         index < destination.constructionSites_.size(); ++index)
    {
        const ProjectConstructionSite& site = destination.constructionSites_[index];
        destination.constructionSiteIndex_.record(site.callableOrdinal, site.syntax, index);
        destination.nextConstructionSequenceByCallable_[site.callableOrdinal] = std::max(
            destination.nextConstructionSequenceByCallable_[site.callableOrdinal],
            site.sequenceWithinCallable + 1U);
    }
    for (std::size_t index = callBase; index < destination.calls_.size(); ++index)
    {
        const MethodCallBinding& binding = destination.calls_[index];
        destination.callBindingIndex_.record(
            binding.callerMethodMemberOrdinal, binding.syntax, index);
    }
#define NSC_INDEX_BY_SYNTAX(field, base, indexField) \
    for (std::size_t index = base; index < destination.field.size(); ++index) \
        destination.indexField.insert_or_assign(destination.field[index].syntax, index)
    NSC_INDEX_BY_SYNTAX(
        aggregateInitializerBindings_, aggregateBase, aggregateInitializerBindingBySyntax_);
    NSC_INDEX_BY_SYNTAX(
        deconstructionBindings_, deconstructionBase, deconstructionBindingBySyntax_);
    NSC_INDEX_BY_SYNTAX(
        matchExpressionBindings_, matchBase, matchExpressionBindingBySyntax_);
    NSC_INDEX_BY_SYNTAX(
        loopExpressionBindings_, loopExpressionBase, loopExpressionBindingBySyntax_);
    NSC_INDEX_BY_SYNTAX(loopBreakBindings_, loopBreakBase, loopBreakBindingBySyntax_);
    NSC_INDEX_BY_SYNTAX(
        namespaceFunctionCalls_, namespaceCallBase, namespaceFunctionCallBySyntax_);
#undef NSC_INDEX_BY_SYNTAX
}
}
