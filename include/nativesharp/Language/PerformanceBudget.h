#pragma once

#include <cstddef>

namespace nativesharp::language
{
struct PerformanceBudget final
{
    std::size_t frontendMilliseconds = 0;
    std::size_t peakMemoryBytes = 0;
    std::size_t mirInstructions = 0;
    std::size_t monomorphizations = 0;
    std::size_t codeBytes = 0;
    std::size_t backendMilliseconds = 0;
    std::size_t toolchainMilliseconds = 0;
};
}
