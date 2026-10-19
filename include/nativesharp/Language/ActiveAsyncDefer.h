#pragma once

#include <cstddef>
#include <string>

namespace nativesharp::language
{
struct ActiveAsyncDefer final
{
    std::string name;
    std::size_t depth = 0;
};
}
