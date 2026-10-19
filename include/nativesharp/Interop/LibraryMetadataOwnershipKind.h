#pragma once

#include <cstdint>

namespace nativesharp::interop
{
enum class LibraryMetadataOwnershipKind : std::uint8_t
{
    None = 0,
    Owned = 1,
    Borrowed = 2
};
}
