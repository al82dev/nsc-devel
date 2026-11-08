#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <optional>
#include <string_view>

namespace nativesharp::frontend::method_body_detail
{
inline std::size_t cppVolatilePointerArithmeticAlignment(
    bool cVolatileContract, std::size_t sourceAlignment, std::string_view elementType) noexcept
{
    if (!cVolatileContract || !sourceAlignment) return 1;
    const std::size_t stride =
        (elementType == "builtin:bool" || elementType == "builtin:sbyte" || elementType == "builtin:byte") ? 1 :
        (elementType == "builtin:short" || elementType == "builtin:ushort") ? 2 :
        (elementType == "builtin:int" || elementType == "builtin:uint" || elementType == "builtin:float") ? 4 :
        (elementType == "builtin:long" || elementType == "builtin:ulong" || elementType == "builtin:double") ? 8 : 0;
    return stride ? std::gcd(sourceAlignment, stride) : 1;
}

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
