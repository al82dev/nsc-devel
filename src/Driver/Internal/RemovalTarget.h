#pragma once

#include <filesystem>

namespace nativesharp::driver
{
namespace
{
struct RemovalTarget final
{
    std::filesystem::path path;
};
} // namespace
} // namespace nativesharp::driver
