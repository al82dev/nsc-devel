#pragma once

#include <filesystem>
#include <string>

namespace nativesharp::driver
{
namespace
{
struct InputIdentity final
{
    std::string role;
    std::filesystem::path path;
};
} // namespace
} // namespace nativesharp::driver
