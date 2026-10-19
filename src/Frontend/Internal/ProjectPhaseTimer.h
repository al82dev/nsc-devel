#pragma once

#include "nativesharp/Basic/CompilationEvent.h"

#include <chrono>
#include <functional>
#include <string_view>

namespace nativesharp::frontend::internal
{
class ProjectPhaseTimer final
{
public:
    ProjectPhaseTimer(
        const std::function<void(std::string_view, std::chrono::nanoseconds)>& timingSink,
        const std::function<void(const basic::CompilationEvent&)>& progressSink,
        std::string_view phase);

    ~ProjectPhaseTimer();

private:
    const std::function<void(std::string_view, std::chrono::nanoseconds)>& timingSink_;
    const std::function<void(const basic::CompilationEvent&)>& progressSink_;
    std::string_view phase_;
    std::chrono::steady_clock::time_point started_;
};
}
