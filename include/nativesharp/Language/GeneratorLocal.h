#pragma once

#include <cstddef>

namespace nativesharp::language
{
struct GeneratorLocal final
{
    std::size_t size;
    std::size_t alignment;
    bool livesAcrossYield;
    bool requiresDrop = false;
    bool moveOnly = false;
    bool borrowed = false;
    // Inclusive suspension-state interval. Zero means the legacy frame-only query did not
    // provide state liveness and is accepted only by computeGeneratorFrame().
    std::size_t firstLiveState = 0;
    std::size_t lastLiveState = 0;
};
}
