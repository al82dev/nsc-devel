#pragma once

#include <filesystem>
#include <functional>

namespace nativesharp::frontend
{
struct ComptimeExecutionContext final
{
    std::filesystem::path sourceDirectory;
    bool hermetic = true;
    std::function<bool()> cancellationRequested;
};
}
