#include "DriverPhaseTimer.h"

#include "DriverExecutionScope.h"

namespace nativesharp::driver
{
DriverPhaseTimer::DriverPhaseTimer(std::string_view phase)
    : scope_(activeExecutionScope != nullptr &&
                     (activeExecutionScope->reportTiming || activeExecutionScope->reportProgress)
                 ? activeExecutionScope
                 : nullptr),
      phase_(phase),
      started_(scope_ != nullptr ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{})
{
    if (scope_ != nullptr)
        scope_->writeProgress({basic::CompilationEventKind::PhaseStarted, phase_, {}, 0, 0, {}});
}

DriverPhaseTimer::~DriverPhaseTimer()
{
    if (scope_ == nullptr)
        return;
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started_);
    scope_->recordPhase(phase_, elapsed);
    scope_->writeProgress({basic::CompilationEventKind::PhaseFinished, phase_, {}, 0, 0, elapsed});
}
} // namespace nativesharp::driver
