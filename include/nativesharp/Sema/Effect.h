#pragma once

#include <cstdint>

namespace nativesharp::sema
{
enum class Effect : std::uint32_t
{
    Unsafe = 1U << 0U,
    Throws = 1U << 1U,
    Async = 1U << 2U,
    Comptime = 1U << 3U,
    NoHeap = 1U << 4U,
    Generator = 1U << 5U
};
}
