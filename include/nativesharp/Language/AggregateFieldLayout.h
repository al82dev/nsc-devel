#pragma once

#include <cstddef>

namespace nativesharp::language
{
struct AggregateFieldLayout final
{
    std::size_t size = 0;
    std::size_t alignment = 1;
};
}
