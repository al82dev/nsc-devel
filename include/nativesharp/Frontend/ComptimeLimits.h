#pragma once

#include <cstddef>

namespace nativesharp::frontend
{
struct ComptimeLimits final
{
    std::size_t maximumSourceBytes = 64U * 1024U * 1024U;
    std::size_t maximumDirectives = 1024;
    std::size_t maximumSteps = 100'000;
    std::size_t maximumNesting = 128;
    std::size_t maximumMemoryBytes = 128U * 1024U * 1024U;
    std::size_t maximumMacroExpansions = 64;
};
}
