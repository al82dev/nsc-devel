#pragma once

#include <cstddef>

namespace nativesharp::language
{
struct SimdContract final { std::size_t laneBits; std::size_t laneCount; bool mask; };
}
