#pragma once

#include <cstdint>

namespace nativesharp::language
{
enum class SmartPointerFamily : std::uint8_t
{
    Unique,
    Rc,
    WeakRc,
    Arc,
    WeakArc
};
}
