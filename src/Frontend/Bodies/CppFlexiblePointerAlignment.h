#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <optional>

namespace nativesharp::frontend::method_body_detail
{
inline std::size_t cppFlexiblePointerKnownAlignment(
    bool pointerReceiver, std::size_t receiverAlignment,
    std::optional<std::uint64_t> fieldAlignment,
    std::optional<std::uint64_t> fieldOffset) noexcept
{
    if (!pointerReceiver || !receiverAlignment || !fieldAlignment || !*fieldAlignment || !fieldOffset)
        return 1;
    return std::min<std::size_t>(static_cast<std::size_t>(*fieldAlignment),
        std::gcd(receiverAlignment, static_cast<std::size_t>(*fieldOffset)));
}
}
