#pragma once

#include <cstdint>

namespace nativesharp::interop
{
enum class LibraryMetadataParameterPassKind : std::uint8_t
{
    Value = 0,
    Ref = 1,
    Out = 2,
    In = 3
};
}
