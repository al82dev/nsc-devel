#pragma once

#include <string>

namespace nativesharp::driver
{
namespace
{
struct CompilationTimeTracePhase final
{
    std::string name;
    long long durationMicroseconds = 0;
    long long peakWorkingSetBytes = 0;
};
} // namespace
} // namespace nativesharp::driver
