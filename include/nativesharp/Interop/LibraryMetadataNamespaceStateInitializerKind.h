#pragma once

#include <cstdint>

namespace nativesharp::interop
{
enum class LibraryMetadataNamespaceStateInitializerKind : std::uint8_t
{
    Zero = 0,
    Integer = 1,
    Floating = 2,
    Boolean = 3,
    String = 4,
    Null = 5
};
}
