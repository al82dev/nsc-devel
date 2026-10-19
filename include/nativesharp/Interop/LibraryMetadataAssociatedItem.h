#pragma once

#include "nativesharp/Interop/LibraryMetadataAssociatedItemKind.h"

#include <cstdint>
#include <string>

namespace nativesharp::interop
{
struct LibraryMetadataAssociatedItem final
{
    LibraryMetadataAssociatedItemKind kind = LibraryMetadataAssociatedItemKind::Type;
    std::string name;
    // Associated type: canonical default/definition type key, empty for a requirement.
    // Associated constant: canonical declared value type key.
    std::string typeKey;
    bool hasDefault = false;
    // Normalized typed integral/boolean bits for a constant default/definition. Zero otherwise.
    std::uint64_t valueBits = 0;
};
}
