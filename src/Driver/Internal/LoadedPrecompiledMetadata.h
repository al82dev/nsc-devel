#pragma once

#include "nativesharp/Interop/LibraryMetadata.h"

#include <filesystem>

namespace nativesharp::driver
{
namespace
{
struct LoadedPrecompiledMetadata final
{
    std::filesystem::path ownerProjectFile;
    std::filesystem::path metadataPath;
    interop::LibraryMetadata metadata;
};
} // namespace
} // namespace nativesharp::driver
