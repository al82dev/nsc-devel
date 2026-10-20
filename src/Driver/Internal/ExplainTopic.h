#pragma once

#include <string_view>

namespace nativesharp::driver
{
struct ExplainTopic final
{
    std::string_view status;
    std::string_view present;
    std::string_view missing;
};
} // namespace nativesharp::driver
