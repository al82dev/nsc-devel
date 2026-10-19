#pragma once

#include <cstdint>

namespace nativesharp::language
{
struct NumericConstant final
{
    std::uint64_t magnitude = 0;
    bool negative = false;
    bool floating = false;
};
}
