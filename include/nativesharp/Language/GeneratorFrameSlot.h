#pragma once

#include <cstddef>

namespace nativesharp::language
{
struct GeneratorFrameSlot final
{
    std::size_t localIndex = 0;
    std::size_t offset = 0;
    std::size_t size = 0;
    std::size_t alignment = 1;
    std::size_t firstLiveState = 0;
    std::size_t lastLiveState = 0;
    bool requiresDrop = false;
    bool moveOnly = false;
    bool borrowed = false;
};
}
