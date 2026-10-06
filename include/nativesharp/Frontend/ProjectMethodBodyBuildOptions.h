#pragma once

#include <cstddef>
#include <functional>
#include <string>

namespace nativesharp::frontend
{
struct ProjectMethodBodyBuildOptions final
{
    std::size_t workerCount = 1;
    std::function<bool()> cancellationCheck;
    std::string* executionError = nullptr;
};
}
