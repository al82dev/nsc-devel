#pragma once

#include <cstddef>

namespace nativesharp::language
{
struct VariantLayout final
{
    std::size_t tagSize = 0;
    std::size_t payloadOffset = 0;
    std::size_t size = 0;
    std::size_t alignment = 1;
};
}
