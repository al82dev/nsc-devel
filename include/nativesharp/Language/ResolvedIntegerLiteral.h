#pragma once

#include <cstdint>

#include "nativesharp/Language/IntegerType.h"

namespace nativesharp::language
{
struct ResolvedIntegerLiteral final
{
    std::uint64_t magnitude = 0;
    bool negative = false;
    IntegerType type;
    bool usedContext = false;
};
}
