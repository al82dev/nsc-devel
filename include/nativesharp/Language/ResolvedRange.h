#pragma once

#include <cstddef>

namespace nativesharp::language
{
struct ResolvedRange final
{
    std::size_t start = 0;
    std::size_t end = 0;
};
}
