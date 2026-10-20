#pragma once

#include "nativesharp/Interop/LibraryMetadata.h"

#include <filesystem>

namespace nativesharp::driver
{
struct LoadedPrecompiledMetadata final
{
    std::filesystem::path ownerProjectFile;
    std::filesystem::path metadataPath;
    interop::LibraryMetadata metadata;
};
} // namespace nativesharp::driver
