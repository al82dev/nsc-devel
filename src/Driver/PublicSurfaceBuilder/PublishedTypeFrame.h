#pragma once

#include <string>

namespace nativesharp::driver
{
struct PublishedTypeFrame final
{
    std::string identity;
    bool expanded = false;
};
} // namespace nativesharp::driver
