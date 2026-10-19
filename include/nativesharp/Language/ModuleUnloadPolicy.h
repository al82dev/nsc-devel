#pragma once

#include <cstdint>

namespace nativesharp::language
{
enum class ModuleUnloadPolicy : std::uint8_t
{
    RejectWhileObjectsLive,
    PinUntilObjectsDestroyed
};
}
