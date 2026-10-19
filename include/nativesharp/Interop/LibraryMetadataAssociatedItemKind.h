#pragma once

#include <cstdint>

namespace nativesharp::interop
{
enum class LibraryMetadataAssociatedItemKind : std::uint8_t
{
    Type = 0,
    Constant = 1
};
}
