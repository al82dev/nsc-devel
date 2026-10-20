#include "CachePruneMeasurement.h"

#include <cstdint>

namespace nativesharp::llvm_backend
{
CachePruneMeasurement::CachePruneMeasurement(CachePruneTelemetry& telemetry)
    : telemetry_(telemetry), started_(std::chrono::steady_clock::now())
{
    telemetry_ = CachePruneTelemetry{};
    telemetry_.performed = true;
}

CachePruneMeasurement::~CachePruneMeasurement()
{
    telemetry_.elapsedMicros = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started_).count());
}
}
