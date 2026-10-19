#pragma once

#include <cstddef>

namespace nativesharp::language
{
struct RangeEndpoint final
{
    std::size_t value = 0;
    bool fromEnd = false;
};
}
