#pragma once

#include <string_view>
#include <vector>

namespace nativesharp::driver
{
struct CommonArguments final
{
    bool json = false;
    std::vector<std::string_view> values;
};
} // namespace nativesharp::driver
