#pragma once

#include "nativesharp/Interop/LibraryMetadata.h"

#include <iosfwd>

namespace nativesharp::interop
{
class LibraryAbiReporter final
{
public:
    static void write(const LibraryMetadata& metadata, std::ostream& output);
};
}
