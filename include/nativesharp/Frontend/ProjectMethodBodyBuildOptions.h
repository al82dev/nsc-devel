#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace nativesharp::frontend
{
struct ProjectMethodBodyBuildOptions final
{
    std::size_t workerCount = 1;
    std::function<bool()> cancellationCheck;
    std::function<void(std::string_view, std::chrono::nanoseconds)> phaseTimingSink;
    std::string* executionError = nullptr;
};
}
