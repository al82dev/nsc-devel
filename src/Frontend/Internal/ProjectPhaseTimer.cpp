#include "ProjectPhaseTimer.h"

#include <utility>

namespace nativesharp::frontend::internal
{
ProjectPhaseTimer::ProjectPhaseTimer(
    const std::function<void(std::string_view, std::chrono::nanoseconds)>& timingSink,
    const std::function<void(const basic::CompilationEvent&)>& progressSink,
    std::string_view phase)
    : timingSink_(timingSink), progressSink_(progressSink), phase_(phase),
      started_(timingSink || progressSink ? std::chrono::steady_clock::now()
                                          : std::chrono::steady_clock::time_point{})
{
    if (progressSink_)
        progressSink_({basic::CompilationEventKind::PhaseStarted,
            phase_, {}, 0, 0, {}});
}

ProjectPhaseTimer::~ProjectPhaseTimer()
{
    if (!timingSink_ && !progressSink_) return;
    const std::chrono::nanoseconds elapsed =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - started_);
    if (timingSink_) timingSink_(phase_, elapsed);
    if (progressSink_)
        progressSink_({basic::CompilationEventKind::PhaseFinished,
            phase_, {}, 0, 0, elapsed});
}
}
