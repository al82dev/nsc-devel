#pragma once

#include "nativesharp/Basic/ExitCode.h"

#include <functional>
#include <iosfwd>
#include <span>
#include <string_view>

namespace nativesharp::driver
{
using DriverExecutor = std::function<ExitCode(
    std::span<const std::string_view>, std::ostream&, std::ostream&)>;

bool isProjectCommand(std::string_view command) noexcept;

ExitCode runProjectCommand(
    std::span<const std::string_view> arguments,
    std::ostream& output,
    std::ostream& error,
    const DriverExecutor& executor);
}
