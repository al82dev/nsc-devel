#pragma once

#include "nativesharp/LLVM/CachePruneTelemetry.h"

#include <chrono>

namespace nativesharp::llvm_backend
{
class CachePruneMeasurement final
{
public:
    explicit CachePruneMeasurement(CachePruneTelemetry& telemetry);
    ~CachePruneMeasurement();

    CachePruneMeasurement(const CachePruneMeasurement&) = delete;
    CachePruneMeasurement& operator=(const CachePruneMeasurement&) = delete;

private:
    CachePruneTelemetry& telemetry_;
    std::chrono::steady_clock::time_point started_;
};
}
