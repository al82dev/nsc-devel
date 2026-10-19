#include "SpecializationWaveTimer.h"

#include <utility>

namespace nativesharp::frontend::internal
{
SpecializationWaveTimer::SpecializationWaveTimer(GenericTelemetry& telemetry, std::string_view canonicalKey)
    : telemetry_(&telemetry), canonicalKey_(canonicalKey),
      started_(telemetry.enabled ? std::chrono::steady_clock::now()
                                 : std::chrono::steady_clock::time_point{})
{
}

SpecializationWaveTimer::~SpecializationWaveTimer()
{
    if (!telemetry_->enabled) return;
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - started_);
    telemetry_->recordSubPhase("generics.specialization-waves", elapsed);
    telemetry_->noteMaterialization(canonicalKey_, elapsed);
}
}
