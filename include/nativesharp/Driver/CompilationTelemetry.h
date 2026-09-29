#pragma once

#include "nativesharp/Basic/CompilationEvent.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace nativesharp::driver
{
enum class ProgressFormat
{
    Text,
    Json
};

bool parseProgressFormat(std::string_view value, ProgressFormat& format) noexcept;
std::uint64_t currentProcessPeakWorkingSetBytes() noexcept;
std::string renderCompilationEvent(
    const basic::CompilationEvent& event,
    ProgressFormat format);
std::string renderCompilationTimeTraceJson(
    std::string_view timingOutput,
    int exitCode);
}
