#include "nativesharp/Language/IterationSemantics.h"

#include <algorithm>
#include <limits>

namespace nativesharp::language
{
bool validateIteratorProtocol(const IteratorProtocol& protocol, std::string& error)
{
    error.clear();
    if (protocol.itemType.empty() || !protocol.hasMoveNext || !protocol.hasCurrent || !protocol.hasCleanup)
    { error = "foreach requires item type, MoveNext, Current, and cleanup"; return false; }
    if (protocol.lendsFromSource && protocol.mode == IterationMode::Value)
    { error = "a lending iterator must yield by reference"; return false; }
    return true;
}

std::optional<std::size_t> lazyRangeCount(const LazyIntegerRange& range, std::string& error) noexcept
{
    error.clear();
    if (range.step == 0) { error = "range step cannot be zero"; return std::nullopt; }
    if ((range.begin < range.end && range.step < 0) || (range.begin > range.end && range.step > 0))
    { error = "range step moves away from its end"; return std::nullopt; }
    if (range.begin == range.end) return range.inclusive ? 1U : 0U;
    const std::uint64_t distance = range.begin < range.end
        ? static_cast<std::uint64_t>(range.end) - static_cast<std::uint64_t>(range.begin)
        : static_cast<std::uint64_t>(range.begin) - static_cast<std::uint64_t>(range.end);
    const std::uint64_t step = range.step < 0
        ? 0U - static_cast<std::uint64_t>(range.step)
        : static_cast<std::uint64_t>(range.step);
    std::uint64_t count = distance / step + ((distance % step) != 0 ? 1U : 0U);
    if (range.inclusive && distance % step == 0)
    {
        if (count == std::numeric_limits<std::uint64_t>::max())
        { error = "range element count overflows size_t"; return std::nullopt; }
        ++count;
    }
    if (count > std::numeric_limits<std::size_t>::max())
    { error = "range element count overflows size_t"; return std::nullopt; }
    return static_cast<std::size_t>(count);
}

bool rangeMaterializesCollection() noexcept { return false; }

bool comprehensionHasImplicitAllocation(BuilderStorage) noexcept { return false; }

bool mutationAllowedDuringBorrowedIteration(
    const IteratorProtocol& protocol,
    SourceMutation mutation) noexcept
{
    return !protocol.lendsFromSource || mutation == SourceMutation::Element;
}

std::optional<GeneratorFrame> computeGeneratorFrame(
    std::span<const GeneratorLocal> locals,
    bool hasSelfReference) noexcept
{
    GeneratorFrame result{1, 1, hasSelfReference}; // state byte
    for (const GeneratorLocal& local : locals)
    {
        if (!local.livesAcrossYield) continue;
        if (local.size == 0 || local.alignment == 0 || local.alignment > 16 ||
            (local.alignment & (local.alignment - 1U)) != 0) return std::nullopt;
        result.alignment = std::max(result.alignment, local.alignment);
        result.size = (result.size + local.alignment - 1U) / local.alignment * local.alignment;
        result.size += local.size;
    }
    result.size = (result.size + result.alignment - 1U) / result.alignment * result.alignment;
    return result;
}

namespace
{
bool validGeneratorLocal(const GeneratorLocal& local) noexcept
{
    return local.size != 0 && local.alignment != 0 && local.alignment <= 16 &&
        (local.alignment & (local.alignment - 1U)) == 0;
}

bool checkedAlignAndAdvance(
    std::size_t& cursor,
    std::size_t alignment,
    std::size_t size) noexcept
{
    if (cursor > std::numeric_limits<std::size_t>::max() - (alignment - 1U)) return false;
    cursor = (cursor + alignment - 1U) / alignment * alignment;
    if (cursor > std::numeric_limits<std::size_t>::max() - size) return false;
    cursor += size;
    return true;
}
}

std::optional<SynchronousGeneratorPlan> buildSynchronousGeneratorPlan(
    std::span<const GeneratorLocal> locals,
    std::span<const GeneratorSuspension> suspensions,
    GeneratorStorage storage,
    bool hasSelfReference,
    std::string& error) noexcept
{
    error.clear();
    if (suspensions.empty())
    {
        error = "a synchronous generator requires at least one productive yield";
        return std::nullopt;
    }

    SynchronousGeneratorPlan plan;
    plan.storage = storage;
    plan.itemType = suspensions.front().itemType;
    plan.completedState = suspensions.size() + 1U;
    if (plan.itemType.empty())
    {
        error = "a generator yield must have a resolved item type";
        return std::nullopt;
    }

    for (std::size_t index = 0; index < suspensions.size(); ++index)
    {
        const GeneratorSuspension& suspension = suspensions[index];
        const std::size_t expectedState = index + 1U;
        if (suspension.state != expectedState)
        {
            error = "generator suspension states must be contiguous and start at one";
            return std::nullopt;
        }
        if (suspension.itemType != plan.itemType)
        {
            error = "all generator yields must resolve to one item type";
            return std::nullopt;
        }
        if (suspension.kind != GeneratorYieldKind::Value)
        {
            if (!suspension.lifetimeTiedToFrame)
            {
                error = "a borrowed generator yield must be tied to frame-owned or captured storage";
                return std::nullopt;
            }
            plan.yieldsBorrowedItems = true;
        }
    }

    std::size_t cursor = 1U; // stable one-byte state discriminator
    std::size_t frameAlignment = 1U;
    for (std::size_t localIndex = 0; localIndex < locals.size(); ++localIndex)
    {
        const GeneratorLocal& local = locals[localIndex];
        if (!local.livesAcrossYield) continue;
        if (!validGeneratorLocal(local))
        {
            error = "generator local has an invalid size or alignment";
            return std::nullopt;
        }
        if (local.firstLiveState == 0 || local.lastLiveState < local.firstLiveState ||
            local.lastLiveState > suspensions.size())
        {
            error = "generator local liveness must name a valid inclusive suspension interval";
            return std::nullopt;
        }

        const std::size_t offsetBefore = cursor;
        if (!checkedAlignAndAdvance(cursor, local.alignment, local.size))
        {
            error = "generator frame layout overflows size_t";
            return std::nullopt;
        }
        const std::size_t alignedOffset =
            (offsetBefore + local.alignment - 1U) / local.alignment * local.alignment;
        frameAlignment = std::max(frameAlignment, local.alignment);
        plan.slots.push_back(GeneratorFrameSlot{
            localIndex,
            alignedOffset,
            local.size,
            local.alignment,
            local.firstLiveState,
            local.lastLiveState,
            local.requiresDrop,
            local.moveOnly,
            local.borrowed});
    }

    if (cursor > std::numeric_limits<std::size_t>::max() - (frameAlignment - 1U))
    {
        error = "generator frame final alignment overflows size_t";
        return std::nullopt;
    }
    cursor = (cursor + frameAlignment - 1U) / frameAlignment * frameAlignment;
    plan.frame = GeneratorFrame{
        cursor,
        frameAlignment,
        hasSelfReference || plan.yieldsBorrowedItems};

    plan.states.reserve(suspensions.size());
    for (const GeneratorSuspension& suspension : suspensions)
    {
        GeneratorState state;
        state.state = suspension.state;
        state.resumeState = suspension.state == suspensions.size()
            ? plan.completedState : suspension.state + 1U;
        state.cleanupDepth = suspension.cleanupDepth;
        for (std::size_t slot = 0; slot < plan.slots.size(); ++slot)
        {
            if (plan.slots[slot].firstLiveState <= suspension.state &&
                suspension.state <= plan.slots[slot].lastLiveState)
            {
                state.liveSlots.push_back(slot);
            }
        }
        plan.states.push_back(std::move(state));
    }
    return plan;
}

std::vector<std::size_t> generatorCancellationDropOrder(
    const SynchronousGeneratorPlan& plan,
    std::size_t suspendedState) noexcept
{
    std::vector<std::size_t> result;
    const auto state = std::ranges::find_if(plan.states, [&](const GeneratorState& candidate)
    {
        return candidate.state == suspendedState;
    });
    if (state == plan.states.end()) return result;

    for (auto iterator = state->liveSlots.rbegin(); iterator != state->liveSlots.rend(); ++iterator)
    {
        if (*iterator < plan.slots.size() && plan.slots[*iterator].requiresDrop)
            result.push_back(*iterator);
    }
    return result;
}

std::optional<std::size_t> generatorResumeState(
    const SynchronousGeneratorPlan& plan,
    std::size_t currentState) noexcept
{
    if (plan.states.empty() || currentState == plan.completedState) return std::nullopt;
    if (currentState == 0) return plan.states.front().state;
    const auto state = std::ranges::find_if(plan.states, [&](const GeneratorState& candidate)
    {
        return candidate.state == currentState;
    });
    return state == plan.states.end()
        ? std::nullopt : std::optional<std::size_t>(state->resumeState);
}

bool mayFuse(const EvaluationContract& source, const EvaluationContract& adapter) noexcept
{
    return source.lazy && adapter.lazy && source.preservesLeftToRightEffects &&
        adapter.preservesLeftToRightEffects &&
        !(source.category == TraversalCategory::SinglePass &&
          adapter.category == TraversalCategory::MultiPass);
}
}
