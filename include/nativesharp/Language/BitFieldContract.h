#pragma once

#include <cstddef>

namespace nativesharp::language
{
struct BitFieldContract final
{
    std::size_t storageBits = 0;
    std::size_t offset = 0;
    std::size_t width = 0;
    bool isSigned = false;
    bool isVolatile = false;
};
}
