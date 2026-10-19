#pragma once

#include "nativesharp/Interop/LibraryMetadataOwnershipKind.h"

#include <cstdint>
#include <string>

namespace nativesharp::interop
{
struct LibraryMetadataField final
{
    std::string name;
    std::string typeKey;
    LibraryMetadataOwnershipKind ownership = LibraryMetadataOwnershipKind::None;
    std::string lifetimeName;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::uint64_t alignment = 1;
    std::uint32_t flags = 0;
    std::uint64_t bitOffset = 0;
    std::uint64_t bitWidth = 0;
};
}
