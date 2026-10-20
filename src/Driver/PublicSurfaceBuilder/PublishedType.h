#pragma once

#include "nativesharp/Interop/LibraryMetadata.h"

#include <vector>

namespace nativesharp::driver
{
struct PublishedType final
{
    const interop::LibraryMetadataType *type = nullptr;
    std::vector<const interop::LibraryMetadataMember *> methods;
};
} // namespace nativesharp::driver
