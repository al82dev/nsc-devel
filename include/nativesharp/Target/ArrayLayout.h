#pragma once

#include <cstdint>

namespace nativesharp::target
{
struct ArrayLayout
{
    std::uint64_t size;
    std::uint64_t alignment;
    std::uint64_t stride;
    std::uint64_t count;
};
}
