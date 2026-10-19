#pragma once

#include <cstdint>

namespace nativesharp::interop
{
enum class LibraryMetadataTypeKind : std::uint8_t
{
    Class = 0,
    Interface = 1,
    Struct = 2,
    Enum = 3,
    Delegate = 4
};
}
