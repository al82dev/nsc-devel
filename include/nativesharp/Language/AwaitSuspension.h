#pragma once

#include <cstddef>

namespace nativesharp::language
{
struct AwaitSuspension final
{
    std::size_t state = 0;
    std::size_t cleanupDepth = 0;
    bool carriesBorrow = false;
    bool borrowSourceOutlivesFrame = false;
    bool borrowAddressStable = false;
};
}
