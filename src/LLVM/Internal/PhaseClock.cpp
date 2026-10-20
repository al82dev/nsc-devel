#include "PhaseClock.h"

namespace nativesharp::llvm_backend
{
PhaseClock::PhaseClock() : started_(std::chrono::steady_clock::now())
{
}

std::uint64_t PhaseClock::elapsedMicros() const
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started_).count());
}
}
