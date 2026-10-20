#pragma once

#include "nativesharp/Interop/LibraryMetadata.h"

#include <string>
#include <vector>

namespace nativesharp::driver
{
struct AccessorContract final
{
    bool isIndexer = false;
    std::string name;
    const interop::LibraryMetadataMember *getter = nullptr;
    const interop::LibraryMetadataMember *setter = nullptr;
    std::vector<const interop::LibraryMetadataMember *> declarationOrder;
};
} // namespace nativesharp::driver
