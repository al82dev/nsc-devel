#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

namespace nativesharp::frontend
{
// Observation only: neither field proves which storage an unknown view may borrow.
struct ViewUnknownOrigin final
{
    std::optional<std::size_t> producerScopeOrdinal;
    std::optional<std::size_t> observedLocalCount;
    bool operator==(const ViewUnknownOrigin&) const = default;
};

// A bounded history prevents quadratic growth during repeated joins. Once
// collapsed, the unlocated Unknown conservatively subsumes all observations.
inline void appendViewUnknownOrigin(std::vector<ViewUnknownOrigin>& origins, const ViewUnknownOrigin& origin)
{
    constexpr std::size_t limit = 8;
    const ViewUnknownOrigin unlocated{};
    if (std::ranges::find(origins, unlocated) != origins.end()) return;
    if (origin == unlocated || (origins.size() >= limit &&
        std::ranges::find(origins, origin) == origins.end()))
    {
        origins.clear();
        origins.push_back(unlocated);
        return;
    }
    if (std::ranges::find(origins, origin) == origins.end()) origins.push_back(origin);
}
}
