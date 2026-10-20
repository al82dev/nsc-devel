#pragma once

#include <chrono>
#include <string_view>

namespace nativesharp::driver
{
struct DriverExecutionScope;

class DriverPhaseTimer final
{
  public:
    explicit DriverPhaseTimer(std::string_view phase);
    ~DriverPhaseTimer();

  private:
    DriverExecutionScope *scope_;
    std::string_view phase_;
    std::chrono::steady_clock::time_point started_;
};
} // namespace nativesharp::driver
