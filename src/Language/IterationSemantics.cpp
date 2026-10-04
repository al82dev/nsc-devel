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

bool mayFuse(const EvaluationContract& source, const EvaluationContract& adapter) noexcept
{
    return source.lazy && adapter.lazy && source.preservesLeftToRightEffects &&
        adapter.preservesLeftToRightEffects &&
        !(source.category == TraversalCategory::SinglePass &&
          adapter.category == TraversalCategory::MultiPass);
}
}
