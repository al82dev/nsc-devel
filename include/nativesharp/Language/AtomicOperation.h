#pragma once

#include <cstdint>

namespace nativesharp::language
{
enum class AtomicOperation : std::uint8_t
{
    Load,
    Store,
    ReadModifyWrite,
    Fence
};
}
