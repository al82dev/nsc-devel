#pragma once

#include <cstdint>
#include <string>

namespace nativesharp::driver
{
namespace
{
struct ManifestEntry final
{
    std::string digest;
    std::uintmax_t size = 0;
    std::string path;
};
} // namespace
} // namespace nativesharp::driver
