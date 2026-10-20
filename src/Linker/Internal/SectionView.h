#pragma once

#include <cstdint>

namespace nativesharp::linker
{
namespace
{
struct SectionView final
{
    std::uint32_t virtualAddress = 0;
    std::uint32_t virtualSize = 0;
    std::uint32_t rawSize = 0;
    std::uint32_t rawOffset = 0;
};
}
}
