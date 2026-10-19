#pragma once

#include <cstdint>

namespace nativesharp::target
{
struct TypeLayout
{
    std::uint64_t size;
    std::uint64_t alignment;
};
}
