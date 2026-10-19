#pragma once

#include <cstddef>
#include <cstdint>

namespace nativesharp::language
{
struct ConstantAllocation final
{
    std::uint64_t identity = 0;
    std::size_t size = 0;
    std::size_t alignment = 1;
    bool live = true;
};
}
