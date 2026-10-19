#pragma once

#include <cstdint>

namespace nativesharp::language_service
{
struct LineBraceSummary final
{
    std::uint32_t opens = 0;
    std::uint32_t closes = 0;
    bool beginsWithClose = false;
};
}
