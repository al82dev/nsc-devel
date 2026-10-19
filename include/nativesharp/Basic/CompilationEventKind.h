#pragma once

namespace nativesharp::basic
{
enum class CompilationEventKind
{
    PhaseStarted,
    PhaseFinished,
    WorkStarted,
    WorkFinished,
    CacheHit,
    CacheMiss,
    Skipped,
    Failed
};
}
