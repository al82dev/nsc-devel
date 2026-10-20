#pragma once

#include <cstdint>

namespace nativesharp::runtime
{
enum class RuntimeAbiImageKind : std::uint8_t
{
    Executable,
    SupportLibrary,
    EntryPoint
};
}
