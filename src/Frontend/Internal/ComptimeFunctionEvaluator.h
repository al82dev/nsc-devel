#pragma once

#include "nativesharp/Frontend/ComptimeLimits.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace nativesharp::frontend::internal
{
inline constexpr std::size_t maximumComptimeFunctionSourceBytes = 4096;

// Evaluates a restricted, pure source function call without executing host code.
// All work shares the caller's resource counters and cancellation callback.
std::optional<std::int64_t> evaluateComptimeFunction(
    std::string_view source,
    std::string_view expression,
    std::size_t directiveOffset,
    const ComptimeLimits& limits,
    std::size_t& steps,
    std::size_t& memoryUsed,
    const std::function<bool()>& cancellationRequested,
    std::string& error);
}
