#pragma once

#include <chrono>
#include <cstdint>

namespace nativesharp::llvm_backend
{
// NSC305-P02. Monotonic wall timer for per-code-unit phase attribution. Zero allocation; the
// elapsed value is read explicitly so each phase can be summed without touching control flow.
class PhaseClock final
{
public:
    PhaseClock();
    std::uint64_t elapsedMicros() const;

private:
    std::chrono::steady_clock::time_point started_;
};
}
