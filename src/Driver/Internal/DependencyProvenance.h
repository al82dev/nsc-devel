#pragma once

#include <filesystem>
#include <string>

namespace nativesharp::driver
{
struct DependencyProvenance final
{
    std::string kind;
    std::string identity;
    std::filesystem::path path;
    std::string fingerprint;
};
} // namespace nativesharp::driver
