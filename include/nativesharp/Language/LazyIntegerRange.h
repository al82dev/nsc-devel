#pragma once

#include <cstdint>

namespace nativesharp::language
{
struct LazyIntegerRange final
{
    std::int64_t begin = 0;
    std::int64_t end = 0;
    std::int64_t step = 1;
    bool inclusive = false;
};
}
