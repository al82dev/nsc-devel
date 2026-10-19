#pragma once

#include <cstdint>

namespace nativesharp::language_service
{
struct TextPosition final
{
    std::uint32_t line = 0;
    std::uint32_t character = 0;

    friend constexpr bool operator==(TextPosition, TextPosition) noexcept = default;
};
}
