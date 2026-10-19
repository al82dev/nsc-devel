#pragma once

#include <cstddef>

namespace nativesharp::frontend::internal
{
struct CloneDepthGuard final
{
    std::size_t& depth;
    explicit CloneDepthGuard(std::size_t& value);
    ~CloneDepthGuard();
};
}
