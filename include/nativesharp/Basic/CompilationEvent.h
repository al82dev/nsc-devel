#pragma once

#include "nativesharp/Basic/CompilationEventKind.h"

#include <chrono>
#include <cstddef>
#include <string_view>

namespace nativesharp::basic
{
struct CompilationEvent final
{
    CompilationEventKind kind = CompilationEventKind::PhaseStarted;
    std::string_view phase;
    std::string_view detail;
    std::size_t current = 0;
    std::size_t total = 0;
    std::chrono::nanoseconds duration{};
};

constexpr std::string_view compilationEventKindName(CompilationEventKind kind) noexcept
{
    switch (kind)
    {
    case CompilationEventKind::PhaseStarted: return "phase-started";
    case CompilationEventKind::PhaseFinished: return "phase-finished";
    case CompilationEventKind::WorkStarted: return "work-started";
    case CompilationEventKind::WorkFinished: return "work-finished";
    case CompilationEventKind::CacheHit: return "cache-hit";
    case CompilationEventKind::CacheMiss: return "cache-miss";
    case CompilationEventKind::Skipped: return "skipped";
    case CompilationEventKind::Failed: return "failed";
    }
    return "unknown";
}
}
