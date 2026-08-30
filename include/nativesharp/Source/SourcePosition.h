#pragma once

#include <cstdint>

namespace nativesharp::source
{
struct SourcePosition
{
    std::uint32_t line = 1;
    std::uint32_t column = 1;
};
}
