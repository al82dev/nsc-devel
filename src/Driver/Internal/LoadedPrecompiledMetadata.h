#pragma once

#include "nativesharp/Interop/LibraryMetadata.h"

#include <filesystem>
#include <memory>

namespace nativesharp::driver
{
struct LoadedPrecompiledMetadata final
{
    std::filesystem::path ownerProjectFile;
    std::filesystem::path metadataPath;
    std::shared_ptr<const interop::LibraryMetadata> metadata;
};
} // namespace nativesharp::driver
