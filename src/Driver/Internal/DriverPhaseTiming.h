#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace nativesharp::driver
{
struct DriverPhaseTiming final
{
    std::string phase;
    std::chrono::nanoseconds elapsed{};
    std::uint64_t peakWorkingSetBytes = 0;
};
} // namespace nativesharp::driver
