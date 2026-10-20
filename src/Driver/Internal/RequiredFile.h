#pragma once

#include <string_view>

namespace nativesharp::driver
{
namespace
{
struct RequiredFile final
{
    std::string_view relativePath;
    std::string_view component;
};
} // namespace
} // namespace nativesharp::driver
