#pragma once

#include <cstddef>
#include <vector>

namespace nativesharp::language
{
struct GeneratorState final
{
    std::size_t state = 0;
    std::size_t resumeState = 0;
    std::size_t cleanupDepth = 0;
    std::vector<std::size_t> liveSlots;
};
}
