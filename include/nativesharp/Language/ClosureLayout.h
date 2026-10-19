#pragma once

#include <cstddef>

namespace nativesharp::language
{
struct ClosureLayout final { std::size_t size = 0; std::size_t alignment = 1; };
}
