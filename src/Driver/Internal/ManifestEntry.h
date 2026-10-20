#pragma once

#include <cstdint>
#include <string>

namespace nativesharp::driver
{
namespace
{
struct ManifestEntry final
{
    std::string sha256;
    std::uintmax_t size = 0;
    std::string relativePath;
};
} // namespace
} // namespace nativesharp::driver
