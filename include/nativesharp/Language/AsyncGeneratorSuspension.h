#pragma once

#include "nativesharp/Language/AsyncGeneratorSuspensionKind.h"

#include <cstddef>

namespace nativesharp::language
{
struct AsyncGeneratorSuspension final
{
    std::size_t state = 0;
    AsyncGeneratorSuspensionKind kind = AsyncGeneratorSuspensionKind::Await;
    std::size_t cleanupDepth = 0;
    bool carriesBorrow = false;
    bool borrowSourceOutlivesFrame = false;
    bool borrowAddressStable = false;
};
}
