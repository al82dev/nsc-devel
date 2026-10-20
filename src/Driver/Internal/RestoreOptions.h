#pragma once

#include <filesystem>
#include <optional>

namespace nativesharp::driver
{
struct RestoreOptions final
{
    std::optional<std::filesystem::path> project;
    std::optional<std::filesystem::path> source;
    bool force = false;
    bool locked = false;
    bool verify = false;
};
} // namespace nativesharp::driver
