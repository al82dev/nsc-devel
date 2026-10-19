#pragma once

#include <cstdint>

namespace nativesharp::interop
{
enum class LibraryMetadataEscapeClass : std::uint8_t
{
    NoEscape = 0,
    Return = 1,
    CallerOwned = 2,
    Global = 3,
    Unknown = 4
};
}
