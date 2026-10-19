#pragma once

#include <cstdint>

namespace nativesharp::language
{
struct IntegerArithmeticResult final
{
    std::uint64_t bits = 0;
    bool overflowed = false;
    bool undefinedBehavior = false;
};
}
