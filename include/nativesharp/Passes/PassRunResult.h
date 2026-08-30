#pragma once

#include <cstddef>

namespace nativesharp::passes
{
struct PassRunResult
{
    bool changed = false;
    std::size_t transformations = 0;
};
}
