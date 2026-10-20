#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace nativesharp::driver
{
struct Artifact final
{
    std::string type;
    std::filesystem::path source;
    std::filesystem::path relativeDestination;
    std::string hash;
    std::uintmax_t size = 0;
};
} // namespace nativesharp::driver
