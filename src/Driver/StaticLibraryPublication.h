#pragma once

#include <filesystem>

namespace nativesharp::driver
{
struct StaticLibraryPublication final
{
    std::filesystem::path destination;
    std::filesystem::path stage;
    std::filesystem::path headerDestination;
    std::filesystem::path headerStage;
    std::filesystem::path metadataDestination;
    std::filesystem::path metadataStage;
    std::filesystem::path productSidecarDestination;
    std::filesystem::path productSidecarStage;
    // ThinLTO sidecar (optimization part 2, item 48); `thinLtoSidecarStaged` says whether this build
    // wrote it. Sidecar ThinLTO (parte 2, punto 48); `thinLtoSidecarStaged` dice si este build lo
    // escribio.
    std::filesystem::path thinLtoSidecarDestination;
    std::filesystem::path thinLtoSidecarStage;
    bool thinLtoSidecarStaged = false;
};
} // namespace nativesharp::driver
