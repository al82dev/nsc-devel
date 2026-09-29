#include "nativesharp/Language/OwnershipSemantics.h"

#include <algorithm>
#include <charconv>
#include <unordered_set>

namespace nativesharp::language
{
bool moveWholeValue(MoveState& state, std::string& error) noexcept
{
    error.clear();
    if (state.isCopy) return true;
    if (std::ranges::none_of(state.initializedFields, [](bool value) { return value; }))
    { error = "use after move"; return false; }
    std::ranges::fill(state.initializedFields, false);
    return true;
}

bool moveField(MoveState& state, std::size_t field, std::string& error) noexcept
{
    error.clear();
    if (state.isCopy) return true;
    if (field >= state.initializedFields.size() || !state.initializedFields[field])
    { error = "field is absent or already moved"; return false; }
    if (state.hasCustomDestructor)
    { error = "partial move is forbidden for a type with a custom destructor"; return false; }
    state.initializedFields[field] = false;
    return true;
}

ValueState classifyValueState(const MoveState& state) noexcept
{
    const auto count = std::ranges::count(state.initializedFields, true);
    if (count == 0) return ValueState::Moved;
    if (count == static_cast<std::ptrdiff_t>(state.initializedFields.size())) return ValueState::Initialized;
    return ValueState::PartiallyMoved;
}

std::size_t requiredDropFlagCount(const MoveState& state) noexcept
{
    return classifyValueState(state) == ValueState::PartiallyMoved ? state.initializedFields.size() : 0U;
}

bool borrowsOverlap(const BorrowPlace& left, const BorrowPlace& right) noexcept
{
    if (left.root != right.root) return false;
    const std::size_t shared = std::min(left.projections.size(), right.projections.size());
    for (std::size_t index = 0; index < shared; ++index)
        if (left.projections[index] != right.projections[index]) return false;
    return true;
}

bool mayTakeExclusiveBorrows(const BorrowPlace& left, const BorrowPlace& right) noexcept
{
    return !borrowsOverlap(left, right);
}

std::optional<std::string> inferElidedOutputLifetime(
    std::span<const std::string> inputLifetimes,
    std::optional<std::string_view> receiverLifetime)
{
    if (receiverLifetime.has_value()) return std::string(*receiverLifetime);
    std::unordered_set<std::string> unique(inputLifetimes.begin(), inputLifetimes.end());
    if (unique.size() == 1) return *unique.begin();
    return std::nullopt;
}

bool outlives(std::span<const std::string> chain, std::string_view longer, std::string_view shorter)
{
    const auto longIt = std::ranges::find(chain, longer);
    const auto shortIt = std::ranges::find(chain, shorter);
    return longIt != chain.end() && shortIt != chain.end() && longIt <= shortIt;
}

std::vector<CleanupAction> cleanupExitOrder(std::span<const CleanupAction> registered)
{
    return {registered.rbegin(), registered.rend()};
}

bool operationPreservesPin(std::string_view operation) noexcept
{
    return operation == "borrow" || operation == "project-pinned-field" || operation == "destroy-in-place";
}

std::string encodeEscapeSummary(const EscapeSummary& summary)
{
    std::string result = "escape-v1|" + std::to_string(summary.functionIdentity.size()) + ":" +
        summary.functionIdentity + "|";
    for (EscapeClass item : summary.parameters) result += std::to_string(static_cast<int>(item)) + ",";
    return result;
}

bool decodeEscapeSummary(std::string_view encoded, EscapeSummary& summary, std::string& error)
{
    error.clear();
    summary = {};
    if (!encoded.starts_with("escape-v1|")) { error = "unsupported escape summary version"; return false; }
    encoded.remove_prefix(10);
    const std::size_t colon = encoded.find(':');
    if (colon == std::string_view::npos) { error = "corrupt escape summary identity"; return false; }
    std::size_t nameSize = 0;
    const auto parsed = std::from_chars(encoded.data(), encoded.data() + colon, nameSize);
    encoded.remove_prefix(colon + 1);
    if (parsed.ec != std::errc{} || encoded.size() < nameSize + 1 || encoded[nameSize] != '|')
    { error = "corrupt escape summary length"; return false; }
    summary.functionIdentity.assign(encoded.substr(0, nameSize));
    encoded.remove_prefix(nameSize + 1);
    while (!encoded.empty())
    {
        const std::size_t comma = encoded.find(',');
        if (comma == std::string_view::npos) { error = "corrupt escape class list"; return false; }
        int value = -1;
        const auto item = std::from_chars(encoded.data(), encoded.data() + comma, value);
        if (item.ec != std::errc{} || value < 0 || value > static_cast<int>(EscapeClass::Unknown))
        { error = "invalid escape class"; return false; }
        summary.parameters.push_back(static_cast<EscapeClass>(value));
        encoded.remove_prefix(comma + 1);
    }
    return true;
}
}
