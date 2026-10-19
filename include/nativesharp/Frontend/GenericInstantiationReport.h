#pragma once

#include <cstddef>

namespace nativesharp::frontend
{
struct GenericInstantiationReport final
{
    std::size_t materializedCount = 0;
    std::size_t cacheHitCount = 0;
    std::size_t maximumObservedDepth = 0;
    std::size_t maximumDepth = 0;
    std::size_t maximumCount = 0;
};
}
