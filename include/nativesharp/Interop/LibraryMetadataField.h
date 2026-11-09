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
    // EN: T08-D02. Qualifiers of the pointee of a pointer-typed declaration, which the type key does not
    //     carry: kMetadataPointeeConst and kMetadataPointeeVolatile. Zero for every other type.
    // ES: T08-D02. Calificadores del dato apuntado de una declaracion de tipo puntero, que la clave de tipo
    //     no lleva: kMetadataPointeeConst y kMetadataPointeeVolatile. Cero para cualquier otro tipo.
    std::uint8_t pointeeQualifiers = 0;
};
}
