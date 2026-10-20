#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
constexpr std::uint8_t kLocalMoved = 1U << 0U;
constexpr std::uint8_t kLocalMaybeMoved = 1U << 1U;
constexpr std::uint8_t kLocalInvoked = 1U << 2U;
constexpr std::uint8_t kLocalMaybeInvoked = 1U << 3U;
constexpr std::uint8_t kLocalInvalidBorrow = 1U << 4U;
constexpr std::uint8_t kLocalMaybeInvalidBorrow = 1U << 5U;
constexpr std::uint8_t kLocalAssigned = 1U << 6U;
constexpr std::uint8_t kParameterMoved = 1U << 0U;
constexpr std::uint8_t kParameterMaybeMoved = 1U << 1U;

std::optional<std::size_t> snapshotLocalIndex(
    const OwnershipFlowSnapshot& snapshot,
    std::size_t globalLocalOrdinal) noexcept
{
    if (globalLocalOrdinal < snapshot.localBaseOrdinal ||
        globalLocalOrdinal >= snapshot.localCount)
    {
        return std::nullopt;
    }
    const std::size_t index = globalLocalOrdinal - snapshot.localBaseOrdinal;
    return index < snapshot.localFlags.size() ? std::optional<std::size_t>(index) : std::nullopt;
}
std::optional<std::size_t> snapshotStorageIndex(
    const OwnershipFlowSnapshot& snapshot,
    std::size_t storageKey) noexcept
{
    if ((storageKey & 1U) != 0U)
    {
        const std::size_t parameterOrdinal = storageKey / 2U;
        return parameterOrdinal < snapshot.parameterFlags.size()
            ? std::optional<std::size_t>(parameterOrdinal)
            : std::nullopt;
    }

    const std::optional<std::size_t> localIndex = snapshotLocalIndex(
        snapshot,
        storageKey / 2U);
    return localIndex.has_value()
        ? std::optional<std::size_t>(snapshot.parameterFlags.size() + *localIndex)
        : std::nullopt;
}

const OwnershipFlowStorageFacts* snapshotStorageFacts(
    const OwnershipFlowSnapshot& snapshot,
    std::size_t storageKey) noexcept
{
    const std::optional<std::size_t> index = snapshotStorageIndex(snapshot, storageKey);
    return index.has_value() && *index < snapshot.storageFacts.size()
        ? &snapshot.storageFacts[*index]
        : nullptr;
}

}

std::size_t ProjectMethodBodyModel::createScope(
    MethodScopeKind kind,
    std::size_t methodMemberOrdinal,
    std::optional<std::size_t> parentScopeOrdinal)
{
    MethodScope scope;
    scope.kind = kind;
    scope.methodMemberOrdinal = methodMemberOrdinal;
    scope.parentScopeOrdinal = parentScopeOrdinal;
    if (parentScopeOrdinal.has_value() && *parentScopeOrdinal < scopes_.size())
    {
        scope.depth = scopes_[*parentScopeOrdinal].depth + 1;
    }

    const std::size_t ordinal = scopes_.size();
    scopes_.push_back(std::move(scope));
    scopeNames_.emplace_back();
    return ordinal;
}

bool ProjectMethodBodyModel::scopeContains(
    std::size_t ancestorScopeOrdinal,
    std::size_t scopeOrdinal) const noexcept
{
    if (ancestorScopeOrdinal >= scopes_.size() || scopeOrdinal >= scopes_.size())
    {
        return false;
    }

    std::optional<std::size_t> current = scopeOrdinal;
    while (current.has_value())
    {
        if (*current == ancestorScopeOrdinal)
        {
            return true;
        }
        current = scopes_[*current].parentScopeOrdinal;
    }
    return false;
}

OwnershipFlowSnapshot ProjectMethodBodyModel::captureOwnershipFlow() const
{
    ++ownershipFlowStatistics_.snapshotsCaptured;
    OwnershipFlowSnapshot state;
    state.reachable = ownershipFlowReachable_;
    state.localBaseOrdinal = currentCallableLocalBase_;
    state.localCount = locals_.size();
    const std::size_t localSlotCount = state.localCount >= state.localBaseOrdinal
        ? state.localCount - state.localBaseOrdinal
        : 0;
    state.localFlags.resize(localSlotCount);
    ownershipFlowStatistics_.denseLocalSlotsCaptured += localSlotCount;
    for (std::size_t localIndex = 0; localIndex < localSlotCount; ++localIndex)
    {
        const std::size_t ordinal = state.localBaseOrdinal + localIndex;
        std::uint8_t flags = 0;
        if (movedRaiiLocals_.contains(ordinal)) flags |= kLocalMoved;
        if (maybeMovedRaiiLocals_.contains(ordinal)) flags |= kLocalMaybeMoved;
        if (invokedFnOnceLocals_.contains(ordinal)) flags |= kLocalInvoked;
        if (maybeInvokedFnOnceLocals_.contains(ordinal)) flags |= kLocalMaybeInvoked;
        if (invalidLocalBorrows_.contains(ordinal)) flags |= kLocalInvalidBorrow;
        if (maybeInvalidLocalBorrows_.contains(ordinal)) flags |= kLocalMaybeInvalidBorrow;
        if (assignedLocals_.contains(ordinal)) flags |= kLocalAssigned;
        state.localFlags[localIndex] = flags;
    }

    state.parameterFlags.resize(currentCallableParameterCount_);
    for (std::size_t parameterOrdinal = 0;
         parameterOrdinal < currentCallableParameterCount_;
         ++parameterOrdinal)
    {
        std::uint8_t flags = 0;
        if (movedOwnedParameters_.contains(parameterOrdinal)) flags |= kParameterMoved;
        if (maybeMovedOwnedParameters_.contains(parameterOrdinal)) flags |= kParameterMaybeMoved;
        state.parameterFlags[parameterOrdinal] = flags;
    }

    state.partiallyMovedPlaces = partiallyMovedPlaces_;
    state.maybePartiallyMovedPlaces = maybePartiallyMovedPlaces_;
    state.assignedRequiredFields = activeRequiredFieldAssignments_;
    state.hasRequiredConstructorExit = hasRequiredConstructorExit_;
    state.requiredConstructorExitAssignments = requiredConstructorExitAssignments_;

    state.storageFacts.resize(state.parameterFlags.size() + state.localFlags.size());
    ownershipFlowStatistics_.denseStorageSlotsCaptured += state.storageFacts.size();
    ownershipFlowStatistics_.partialPlaceEntriesCopied +=
        state.partiallyMovedPlaces.size() + state.maybePartiallyMovedPlaces.size();
    const auto captureStorage = [&](std::size_t storageKey)
    {
        const std::optional<std::size_t> storageIndex = snapshotStorageIndex(state, storageKey);
        if (!storageIndex.has_value()) return;
        OwnershipFlowStorageFacts& facts = state.storageFacts[*storageIndex];
        if (const auto active = activeUnionMembers_.find(storageKey);
            active != activeUnionMembers_.end())
            facts.activeUnionMember = active->second;
        facts.nonNull = nonNullStorages_.contains(storageKey);
        if (const auto range = integerRanges_.find(storageKey); range != integerRanges_.end())
            facts.integerRange = range->second;
        if (const auto activeCase = activeVariantCases_.find(storageKey);
            activeCase != activeVariantCases_.end())
            facts.activeVariantCase = activeCase->second;
    };
    for (std::size_t parameterOrdinal = 0;
         parameterOrdinal < state.parameterFlags.size();
         ++parameterOrdinal)
        captureStorage(parameterOrdinal * 2U + 1U);
    for (std::size_t localIndex = 0; localIndex < state.localFlags.size(); ++localIndex)
        captureStorage((state.localBaseOrdinal + localIndex) * 2U);
    return state;
}

void ProjectMethodBodyModel::restoreOwnershipFlow(const OwnershipFlowSnapshot& state)
{
    ++ownershipFlowStatistics_.snapshotsRestored;
    ownershipFlowReachable_ = state.reachable;
    restoreLocalOwnershipFlags(state);
    restoreParameterOwnershipFlags(state);
    partiallyMovedPlaces_ = state.partiallyMovedPlaces;
    maybePartiallyMovedPlaces_ = state.maybePartiallyMovedPlaces;
    activeRequiredFieldAssignments_ = state.assignedRequiredFields;
    hasRequiredConstructorExit_ = state.hasRequiredConstructorExit;
    requiredConstructorExitAssignments_ = state.requiredConstructorExitAssignments;
    restoreAssignedLocals(state);
    std::vector<std::pair<std::size_t, std::size_t>> activeAfterSnapshot;
    for (const auto& [storage, member] : activeUnionMembers_)
    {
        if ((storage & 1U) == 0U && storage / 2U >= state.localCount)
            activeAfterSnapshot.emplace_back(storage, member);
    }
    restoreOwnershipStorageFacts(state, activeAfterSnapshot);
}

void ProjectMethodBodyModel::restoreLocalOwnershipFlags(const OwnershipFlowSnapshot& state)
{
    movedRaiiLocals_.clear();
    maybeMovedRaiiLocals_.clear();
    invokedFnOnceLocals_.clear();
    maybeInvokedFnOnceLocals_.clear();
    invalidLocalBorrows_.clear();
    maybeInvalidLocalBorrows_.clear();
    for (std::size_t localIndex = 0; localIndex < state.localFlags.size(); ++localIndex)
    {
        const std::size_t ordinal = state.localBaseOrdinal + localIndex;
        const std::uint8_t flags = state.localFlags[localIndex];
        if ((flags & kLocalMoved) != 0U) movedRaiiLocals_.insert(ordinal);
        if ((flags & kLocalMaybeMoved) != 0U) maybeMovedRaiiLocals_.insert(ordinal);
        if ((flags & kLocalInvoked) != 0U) invokedFnOnceLocals_.insert(ordinal);
        if ((flags & kLocalMaybeInvoked) != 0U) maybeInvokedFnOnceLocals_.insert(ordinal);
        if ((flags & kLocalInvalidBorrow) != 0U) invalidLocalBorrows_.insert(ordinal);
        if ((flags & kLocalMaybeInvalidBorrow) != 0U)
            maybeInvalidLocalBorrows_.insert(ordinal);
    }
}

void ProjectMethodBodyModel::restoreParameterOwnershipFlags(const OwnershipFlowSnapshot& state)
{
    movedOwnedParameters_.clear();
    maybeMovedOwnedParameters_.clear();
    for (std::size_t parameterOrdinal = 0;
         parameterOrdinal < state.parameterFlags.size();
         ++parameterOrdinal)
    {
        const std::uint8_t flags = state.parameterFlags[parameterOrdinal];
        if ((flags & kParameterMoved) != 0U)
            movedOwnedParameters_.insert(parameterOrdinal);
        if ((flags & kParameterMaybeMoved) != 0U)
            maybeMovedOwnedParameters_.insert(parameterOrdinal);
    }
}

void ProjectMethodBodyModel::restoreAssignedLocals(const OwnershipFlowSnapshot& state)
{
    // C7: a snapshot cannot say anything about a local that did not exist when it was taken, and
    // restoring one must not silently un-assign such a local. Catch clauses are the case that
    // forced this: analyzeTry captures its catch-entry state, THEN declares the exception local
    // (which the handler itself binds, so it starts out assigned), THEN restores -- and a plain
    // assignment of the set would have thrown that binding away and reported every use of a
    // caught exception as a read of an unassigned variable. Local ordinals only ever grow, so
    // "declared after this snapshot" is exactly "ordinal >= its local count". Carrying those bits
    // across cannot mask a real error: a local declared after the snapshot point is, by lexical
    // scoping, not nameable on the path being restored.
    std::vector<std::size_t> declaredAfterSnapshot;
    for (const std::size_t ordinal : assignedLocals_)
    {
        if (ordinal >= state.localCount)
        {
            declaredAfterSnapshot.push_back(ordinal);
        }
    }
    assignedLocals_.clear();
    for (std::size_t localIndex = 0; localIndex < state.localFlags.size(); ++localIndex)
    {
        if ((state.localFlags[localIndex] & kLocalAssigned) != 0U)
            assignedLocals_.insert(state.localBaseOrdinal + localIndex);
    }
    assignedLocals_.insert(declaredAfterSnapshot.begin(), declaredAfterSnapshot.end());
}

void ProjectMethodBodyModel::restoreOwnershipStorageFacts(
    const OwnershipFlowSnapshot& state,
    std::span<const std::pair<std::size_t, std::size_t>> activeAfterSnapshot)
{
    activeUnionMembers_.clear();
    nonNullStorages_.clear();
    integerRanges_.clear();
    activeVariantCases_.clear();
    const auto restoreStorage = [&](std::size_t storageKey)
    {
        const OwnershipFlowStorageFacts* facts = snapshotStorageFacts(state, storageKey);
        if (facts == nullptr) return;
        if (facts->activeUnionMember.has_value())
            activeUnionMembers_.emplace(storageKey, *facts->activeUnionMember);
        if (facts->nonNull) nonNullStorages_.insert(storageKey);
        if (facts->integerRange.has_value())
            integerRanges_.emplace(storageKey, *facts->integerRange);
        if (facts->activeVariantCase.has_value())
            activeVariantCases_.emplace(storageKey, *facts->activeVariantCase);
    };
    for (std::size_t parameterOrdinal = 0;
         parameterOrdinal < state.parameterFlags.size();
         ++parameterOrdinal)
        restoreStorage(parameterOrdinal * 2U + 1U);
    for (std::size_t localIndex = 0; localIndex < state.localFlags.size(); ++localIndex)
        restoreStorage((state.localBaseOrdinal + localIndex) * 2U);
    activeUnionMembers_.insert(activeAfterSnapshot.begin(), activeAfterSnapshot.end());
}

// CC1-200. El patron de rama, en un solo sitio. Ver la declaracion en el header para el porque.
//
// Corre `work` partiendo EXACTAMENTE de `entry`, devuelve el estado en el que quedo, y deja el
// estado activo de vuelta en `entry`. El llamador combina dos de estos con commitBranchJoin.
OwnershipFlowSnapshot ProjectMethodBodyModel::analyzeBranchState(
    const OwnershipFlowSnapshot& entry, const std::function<void()>& work)
{
    restoreOwnershipFlow(entry);
    work();
    OwnershipFlowSnapshot exitState = captureOwnershipFlow();
    restoreOwnershipFlow(entry);
    return exitState;
}

void ProjectMethodBodyModel::commitBranchJoin(
    const OwnershipFlowSnapshot& left, const OwnershipFlowSnapshot& right)
{
    restoreOwnershipFlow(joinOwnershipFlow(left, right));
}

OwnershipFlowSnapshot ProjectMethodBodyModel::joinOwnershipFlow(
    const OwnershipFlowSnapshot& left,
    const OwnershipFlowSnapshot& right) const
{
    ++ownershipFlowStatistics_.joinsComputed;
    if (!left.reachable)
    {
        OwnershipFlowSnapshot result = right;
        mergeRequiredOwnershipExits(result, left, right);
        return result;
    }
    if (!right.reachable)
    {
        OwnershipFlowSnapshot result = left;
        mergeRequiredOwnershipExits(result, left, right);
        return result;
    }

    OwnershipFlowSnapshot result;
    result.reachable = true;
    result.localBaseOrdinal = std::min(left.localBaseOrdinal, right.localBaseOrdinal);
    result.localCount = std::max(left.localCount, right.localCount);
    result.localFlags.resize(result.localCount >= result.localBaseOrdinal
        ? result.localCount - result.localBaseOrdinal
        : 0);
    joinLocalOwnershipFlags(result, left, right);
    joinParameterOwnershipFlags(result, left, right);
    joinPartialOwnershipState(left, right, result);
    joinRequiredOwnershipFields(left, right, result);
    mergeRequiredOwnershipExits(result, left, right);
    joinOwnershipStorageFacts(result, left, right);
    return result;
}

void ProjectMethodBodyModel::mergeRequiredOwnershipExits(
    OwnershipFlowSnapshot& result,
    const OwnershipFlowSnapshot& left,
    const OwnershipFlowSnapshot& right)
{
    result.hasRequiredConstructorExit = left.hasRequiredConstructorExit ||
        right.hasRequiredConstructorExit;
    if (!result.hasRequiredConstructorExit)
    {
        result.requiredConstructorExitAssignments.clear();
        return;
    }
    if (!left.hasRequiredConstructorExit)
    {
        result.requiredConstructorExitAssignments = right.requiredConstructorExitAssignments;
        return;
    }
    result.requiredConstructorExitAssignments = left.requiredConstructorExitAssignments;
    if (!right.hasRequiredConstructorExit) return;
    std::erase_if(
        result.requiredConstructorExitAssignments,
        [&](std::size_t fieldOrdinal)
        {
            return !right.requiredConstructorExitAssignments.contains(fieldOrdinal);
        });
}

std::uint8_t ProjectMethodBodyModel::joinedLocalFlagsAt(
    const OwnershipFlowSnapshot& snapshot,
    std::size_t ordinal) noexcept
{
    const std::optional<std::size_t> index = snapshotLocalIndex(snapshot, ordinal);
    return index.has_value() ? snapshot.localFlags[*index] : 0U;
}

std::uint8_t ProjectMethodBodyModel::joinOwnershipTriStateBits(
    std::uint8_t leftFlags,
    std::uint8_t rightFlags,
    std::uint8_t definiteBit,
    std::uint8_t maybeBit) noexcept
{
    const bool definite = (leftFlags & definiteBit) != 0U &&
        (rightFlags & definiteBit) != 0U;
    const bool possible = ((leftFlags | rightFlags) & (definiteBit | maybeBit)) != 0U;
    return definite ? definiteBit : possible ? maybeBit : 0U;
}

void ProjectMethodBodyModel::joinLocalOwnershipFlags(
    OwnershipFlowSnapshot& result,
    const OwnershipFlowSnapshot& left,
    const OwnershipFlowSnapshot& right)
{
    for (std::size_t localIndex = 0; localIndex < result.localFlags.size(); ++localIndex)
    {
        const std::size_t ordinal = result.localBaseOrdinal + localIndex;
        const std::uint8_t leftFlags = joinedLocalFlagsAt(left, ordinal);
        const std::uint8_t rightFlags = joinedLocalFlagsAt(right, ordinal);
        std::uint8_t flags = 0;
        flags |= joinOwnershipTriStateBits(
            leftFlags, rightFlags, kLocalMoved, kLocalMaybeMoved);
        flags |= joinOwnershipTriStateBits(
            leftFlags, rightFlags, kLocalInvoked, kLocalMaybeInvoked);
        flags |= joinOwnershipTriStateBits(
            leftFlags, rightFlags, kLocalInvalidBorrow, kLocalMaybeInvalidBorrow);
        const bool leftAssigned = (leftFlags & kLocalAssigned) != 0U;
        const bool rightAssigned = (rightFlags & kLocalAssigned) != 0U;
        if ((leftAssigned && (rightAssigned || ordinal >= right.localCount)) ||
            (rightAssigned && ordinal >= left.localCount))
        {
            flags |= kLocalAssigned;
        }
        result.localFlags[localIndex] = flags;
    }
}

void ProjectMethodBodyModel::joinParameterOwnershipFlags(
    OwnershipFlowSnapshot& result,
    const OwnershipFlowSnapshot& left,
    const OwnershipFlowSnapshot& right)
{
    result.parameterFlags.resize(std::max(
        left.parameterFlags.size(),
        right.parameterFlags.size()));
    for (std::size_t parameterOrdinal = 0;
         parameterOrdinal < result.parameterFlags.size();
         ++parameterOrdinal)
    {
        const std::uint8_t leftFlags = parameterOrdinal < left.parameterFlags.size()
            ? left.parameterFlags[parameterOrdinal] : 0U;
        const std::uint8_t rightFlags = parameterOrdinal < right.parameterFlags.size()
            ? right.parameterFlags[parameterOrdinal] : 0U;
        result.parameterFlags[parameterOrdinal] = joinOwnershipTriStateBits(
            leftFlags,
            rightFlags,
            kParameterMoved,
            kParameterMaybeMoved);
    }
}

void ProjectMethodBodyModel::joinPartialOwnershipState(
    const OwnershipFlowSnapshot& left,
    const OwnershipFlowSnapshot& right,
    OwnershipFlowSnapshot& result)
{
    for (const std::string& place : left.partiallyMovedPlaces)
        if (right.partiallyMovedPlaces.contains(place))
            result.partiallyMovedPlaces.insert(place);
    std::unordered_set<std::string> possible = left.partiallyMovedPlaces;
    possible.insert(left.maybePartiallyMovedPlaces.begin(), left.maybePartiallyMovedPlaces.end());
    possible.insert(right.partiallyMovedPlaces.begin(), right.partiallyMovedPlaces.end());
    possible.insert(right.maybePartiallyMovedPlaces.begin(), right.maybePartiallyMovedPlaces.end());
    for (const std::string& place : possible)
        if (!result.partiallyMovedPlaces.contains(place))
            result.maybePartiallyMovedPlaces.insert(place);
}

void ProjectMethodBodyModel::joinRequiredOwnershipFields(
    const OwnershipFlowSnapshot& left,
    const OwnershipFlowSnapshot& right,
    OwnershipFlowSnapshot& result)
{
    for (const std::size_t fieldOrdinal : left.assignedRequiredFields)
        if (right.assignedRequiredFields.contains(fieldOrdinal))
            result.assignedRequiredFields.insert(fieldOrdinal);
}

void ProjectMethodBodyModel::joinOwnershipStorageFacts(
    OwnershipFlowSnapshot& result,
    const OwnershipFlowSnapshot& left,
    const OwnershipFlowSnapshot& right)
{
    result.storageFacts.resize(result.parameterFlags.size() + result.localFlags.size());
    for (std::size_t parameterOrdinal = 0;
         parameterOrdinal < result.parameterFlags.size();
         ++parameterOrdinal)
        joinOwnershipStorageFact(result, left, right, parameterOrdinal * 2U + 1U);
    for (std::size_t localIndex = 0; localIndex < result.localFlags.size(); ++localIndex)
        joinOwnershipStorageFact(
            result, left, right, (result.localBaseOrdinal + localIndex) * 2U);
}

void ProjectMethodBodyModel::joinOwnershipStorageFact(
    OwnershipFlowSnapshot& result,
    const OwnershipFlowSnapshot& left,
    const OwnershipFlowSnapshot& right,
    std::size_t storageKey)
{
    const OwnershipFlowStorageFacts* leftFacts = snapshotStorageFacts(left, storageKey);
    const OwnershipFlowStorageFacts* rightFacts = snapshotStorageFacts(right, storageKey);
    const std::optional<std::size_t> resultIndex = snapshotStorageIndex(result, storageKey);
    if (!resultIndex.has_value()) return;
    OwnershipFlowStorageFacts& joinedFacts = result.storageFacts[*resultIndex];
    const bool isLocal = (storageKey & 1U) == 0U;
    const std::size_t localOrdinal = storageKey / 2U;
    const bool localCreatedAfterLeft = isLocal && localOrdinal >= left.localCount;
    const bool localCreatedAfterRight = isLocal && localOrdinal >= right.localCount;
    if (leftFacts != nullptr && leftFacts->activeUnionMember.has_value() &&
        ((rightFacts != nullptr && rightFacts->activeUnionMember ==
            leftFacts->activeUnionMember) || localCreatedAfterRight))
        joinedFacts.activeUnionMember = leftFacts->activeUnionMember;
    else if (rightFacts != nullptr && rightFacts->activeUnionMember.has_value() &&
             localCreatedAfterLeft)
        joinedFacts.activeUnionMember = rightFacts->activeUnionMember;
    joinedFacts.nonNull = leftFacts != nullptr && rightFacts != nullptr &&
        leftFacts->nonNull && rightFacts->nonNull;
    if (leftFacts != nullptr && rightFacts != nullptr &&
        leftFacts->integerRange.has_value() && rightFacts->integerRange.has_value())
    {
        OwnershipFlowIntegerRange range;
        if (leftFacts->integerRange->minimum.has_value() &&
            rightFacts->integerRange->minimum.has_value())
            range.minimum = std::min(
                *leftFacts->integerRange->minimum, *rightFacts->integerRange->minimum);
        if (leftFacts->integerRange->maximum.has_value() &&
            rightFacts->integerRange->maximum.has_value())
            range.maximum = std::max(
                *leftFacts->integerRange->maximum, *rightFacts->integerRange->maximum);
        range.excludesZero = leftFacts->integerRange->excludesZero &&
            rightFacts->integerRange->excludesZero;
        if (range.minimum.has_value() || range.maximum.has_value() || range.excludesZero)
            joinedFacts.integerRange = range;
    }
    if (leftFacts != nullptr && rightFacts != nullptr &&
        leftFacts->activeVariantCase.has_value() &&
        leftFacts->activeVariantCase == rightFacts->activeVariantCase)
        joinedFacts.activeVariantCase = leftFacts->activeVariantCase;
}

OwnershipFlowSnapshot ProjectMethodBodyModel::joinOwnershipFlows(
    const OwnershipFlowSnapshot& seed,
    const std::vector<OwnershipFlowSnapshot>& states) const
{
    OwnershipFlowSnapshot result = seed;
    for (const OwnershipFlowSnapshot& state : states)
    {
        result = joinOwnershipFlow(result, state);
    }
    return result;
}

void ProjectMethodBodyModel::seedParameters(
    std::size_t methodMemberOrdinal,
    std::size_t rootScopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const ClassMemberSymbol* method = methodSymbol(methodMemberOrdinal);
    if (method == nullptr || rootScopeOrdinal >= scopeNames_.size())
    {
        return;
    }

    for (std::size_t parameterOrdinal = 0; parameterOrdinal < method->parameters.size(); ++parameterOrdinal)
    {
        const MethodParameterSymbol& parameter = method->parameters[parameterOrdinal];
        const std::optional<ScopeName> previous = lookupScopeNameInScope(rootScopeOrdinal, parameter.name);
        if (previous.has_value())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                parameter.range,
                "duplicate parameter/local name '" + parameter.name + "' in method root scope");
            reportPrevious(previous->range, diagnostics, "previous declaration is here");
            continue;
        }

        scopeNames_[rootScopeOrdinal].push_back(ScopeName{
            ScopeNameKind::Parameter,
            parameter.name,
            parameterOrdinal,
            parameter.range});

        if (!memberCandidates(methodMemberOrdinal, parameter.name).empty())
        {
            shadowingEvents_.push_back(MethodShadowingEvent{
                MethodShadowingKind::ParameterShadowsMember,
                methodMemberOrdinal,
                rootScopeOrdinal,
                parameter.name,
                parameter.range});
        }
    }
}

std::optional<ProjectMethodBodyModel::ScopeName> ProjectMethodBodyModel::lookupScopeName(
    std::size_t scopeOrdinal,
    std::string_view name) const
{
    std::optional<std::size_t> current = scopeOrdinal;
    while (current.has_value() && *current < scopes_.size())
    {
        const std::optional<ScopeName> found = lookupScopeNameInScope(*current, name);
        if (found.has_value())
        {
            return found;
        }
        current = scopes_[*current].parentScopeOrdinal;
    }
    return std::nullopt;
}

std::optional<ProjectMethodBodyModel::ScopeName> ProjectMethodBodyModel::lookupScopeNameInScope(
    std::size_t scopeOrdinal,
    std::string_view name) const
{
    if (scopeOrdinal >= scopeNames_.size())
    {
        return std::nullopt;
    }
    const std::vector<ScopeName>& names = scopeNames_[scopeOrdinal];
    for (auto iterator = names.rbegin(); iterator != names.rend(); ++iterator)
    {
        if (iterator->name == name)
        {
            return *iterator;
        }
    }
    return std::nullopt;
}
}

