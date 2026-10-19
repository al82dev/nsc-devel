#pragma once

#include <cstdint>

namespace nativesharp::language
{
enum class RecordDerive : std::uint8_t
{
    Equality = 1U << 0U,
    Hash = 1U << 1U,
    Print = 1U << 2U,
    Clone = 1U << 3U
};
}
