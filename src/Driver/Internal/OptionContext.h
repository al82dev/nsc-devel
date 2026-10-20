#pragma once

#include <cstddef>
#include <iosfwd>
#include <span>
#include <string_view>

namespace nativesharp::driver
{
class CompilerInvocation;

namespace
{
struct OptionContext final
{
    std::span<const std::string_view> arguments;
    std::size_t &index;
    std::string_view argument;
    std::string_view folded;
    CompilerInvocation &invocation;
    std::ostream &error;
};
} // namespace
} // namespace nativesharp::driver
