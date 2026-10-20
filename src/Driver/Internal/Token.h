#pragma once

#include <cstdint>
#include <string_view>

namespace nativesharp::driver
{
namespace
{
struct Token final
{
    std::string_view text;
    std::uint32_t offset = 0;
    std::uint32_t line = 1;
    std::uint32_t column = 1;
    bool identifier = false;
    bool number = false;
};
} // namespace
} // namespace nativesharp::driver
