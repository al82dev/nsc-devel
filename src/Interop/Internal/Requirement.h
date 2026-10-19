#pragma once

#include <span>
#include <string_view>

namespace nativesharp::interop
{
namespace
{
struct Requirement final
{
    std::string_view id;
    std::string_view name;
    std::span<const std::string_view> types;
};
}
}
