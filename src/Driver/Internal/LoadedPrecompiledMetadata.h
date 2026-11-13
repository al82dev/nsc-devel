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
    std::filesystem::path productSidecarPath;
    std::filesystem::path libraryPath;
    bool staticLibrary = false;
    bool ambiguousLibrary = false;
    bool receiverContractGuarded = false;
};
} // namespace nativesharp::driver
