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
};
}
