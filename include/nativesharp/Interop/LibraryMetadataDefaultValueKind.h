#pragma once

#include <cstdint>

namespace nativesharp::interop
{
enum class LibraryMetadataDefaultValueKind : std::uint8_t
{
    None = 0,
    Null = 1,
    Boolean = 2,
    Integer = 3,
    Floating = 4,
    Character = 5,
    String = 6
};
}
