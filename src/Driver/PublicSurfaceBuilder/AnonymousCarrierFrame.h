#pragma once

#include "nativesharp/Interop/LibraryMetadata.h"

#include <cstddef>
#include <string>

namespace nativesharp::driver
{
struct AnonymousCarrierFrame final
{
    const interop::LibraryMetadataType *owner = nullptr;
    const interop::LibraryMetadataField *carrier = nullptr;
    const interop::LibraryMetadataType *nested = nullptr;
    std::string indentation;
    std::size_t nextChild = 0;
};
} // namespace nativesharp::driver
