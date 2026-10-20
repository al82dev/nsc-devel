#pragma once

#include <string>

namespace nativesharp::driver
{
struct PackageRequest final
{
    std::string id;
    std::string version;
    std::string source;
};
} // namespace nativesharp::driver
