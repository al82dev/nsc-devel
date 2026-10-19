#pragma once

#include <cstddef>
#include <vector>

namespace nativesharp::language
{
struct AggregateLayout final
{
    std::vector<std::size_t> offsets;
    std::size_t size = 0;
    std::size_t alignment = 1;
};
}
