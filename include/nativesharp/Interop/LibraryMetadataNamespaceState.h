#pragma once

#include "nativesharp/Interop/LibraryMetadataNamespaceStateInitializerKind.h"

#include <cstdint>
#include <string>

namespace nativesharp::interop
{
struct LibraryMetadataNamespaceState final
{
    std::uint8_t accessibility = 0;
    std::uint8_t flags = 0;
    LibraryMetadataNamespaceStateInitializerKind initializerKind =
        LibraryMetadataNamespaceStateInitializerKind::Zero;
    std::string namespaceName;
    std::string name;
    std::string declaredTypeKey;
    std::string storageName;
    std::string accessorName;
    std::uint64_t initializerBits = 0;
    std::string initializerText;
    // EN: T08-D02. Qualifiers of the pointee of a pointer-typed declaration, which the type key does not
    //     carry: kMetadataPointeeConst and kMetadataPointeeVolatile. Zero for every other type.
    // ES: T08-D02. Calificadores del dato apuntado de una declaracion de tipo puntero, que la clave de tipo
    //     no lleva: kMetadataPointeeConst y kMetadataPointeeVolatile. Cero para cualquier otro tipo.
    std::uint8_t declaredPointeeQualifiers = 0;
};
}
