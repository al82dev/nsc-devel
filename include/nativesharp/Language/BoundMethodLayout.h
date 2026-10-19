#pragma once

#include <cstddef>

namespace nativesharp::language
{
struct BoundMethodLayout final { std::size_t size; std::size_t alignment; };
}
