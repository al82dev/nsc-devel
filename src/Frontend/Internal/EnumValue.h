#pragma once

#include <cstdint>

namespace nativesharp::frontend::internal
{
struct EnumValue
{
    std::uint64_t magnitude = 0;
    bool negative = false;
};
}
