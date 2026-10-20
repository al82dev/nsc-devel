#pragma once

#include <filesystem>

namespace nativesharp::driver
{
struct ObjectPublication final
{
    std::filesystem::path destination;
    std::filesystem::path stage;
};
} // namespace nativesharp::driver
