#pragma once

#include <cstdint>

namespace nativesharp::language
{
enum class SmartPointerOperation : std::uint8_t
{
    Move,
    ImplicitCopy,
    Clone,
    Downgrade,
    Upgrade,
    Release,
    Reset,
    CrossThread
};
}
