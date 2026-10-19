#pragma once

#include "nativesharp/Frontend/GenericTelemetry.h"

#include <chrono>
#include <string_view>

namespace nativesharp::frontend::internal
{
class SpecializationWaveTimer final
{
public:
    SpecializationWaveTimer(GenericTelemetry& telemetry, std::string_view canonicalKey);
    ~SpecializationWaveTimer();
    SpecializationWaveTimer(const SpecializationWaveTimer&) = delete;
    SpecializationWaveTimer& operator=(const SpecializationWaveTimer&) = delete;

private:
    GenericTelemetry* telemetry_;
    std::string_view canonicalKey_;
    std::chrono::steady_clock::time_point started_;
};
}
