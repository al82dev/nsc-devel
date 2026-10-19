#pragma once

#include <cstddef>
#include <string>

#include "nativesharp/Language/GeneratorYieldKind.h"

namespace nativesharp::language
{
struct GeneratorSuspension final
{
    // State zero is the entry state. Productive yields are numbered contiguously from one;
    // the completed state is suspension-count + one.
    std::size_t state = 0;
    std::string itemType;
    GeneratorYieldKind kind = GeneratorYieldKind::Value;
    std::size_t cleanupDepth = 0;
    // Reference yields may only lend storage owned by the frame or by a captured source whose
    // lifetime is carried by the frame. This marker is supplied after borrow analysis.
    bool lifetimeTiedToFrame = false;
};
}
