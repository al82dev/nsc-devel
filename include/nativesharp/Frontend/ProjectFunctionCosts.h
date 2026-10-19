#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
struct ProjectFunctionCosts final
{
    std::string function;
    std::uint64_t staticStackBytesLowerBound = 0;
    std::uint64_t arenaAllocationSites = 0;
    std::uint64_t heapAllocationSites = 0;
    std::uint64_t copySites = 0;
    std::uint64_t moveSites = 0;
    std::uint64_t dropSites = 0;
    std::uint64_t dynamicDispatchSites = 0;
    std::uint64_t lockSites = 0;
    std::uint64_t atomicSites = 0;
    std::uint64_t unwindSites = 0;
    std::uint64_t monomorphizations = 0;
    std::vector<std::string> causes;
};
}
