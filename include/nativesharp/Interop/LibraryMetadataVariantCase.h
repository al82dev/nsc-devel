#pragma once

#include <string>

namespace nativesharp::interop
{
struct LibraryMetadataVariantCase final
{
    std::string name;
    // Empty for a unit case; otherwise the canonical type key of the in-place payload.
    std::string payloadTypeKey;
};
}
