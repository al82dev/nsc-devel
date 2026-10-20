#pragma once

#include <filesystem>

namespace nativesharp::driver
{
struct DynamicLibraryPublication final
{
    std::filesystem::path destination;
    std::filesystem::path stage;
    std::filesystem::path importLibraryDestination;
    std::filesystem::path importLibraryStage;
    std::filesystem::path headerDestination;
    std::filesystem::path headerStage;
    std::filesystem::path metadataDestination;
    std::filesystem::path metadataStage;
    std::filesystem::path pdbDestination;
    std::filesystem::path pdbStage;
};
} // namespace nativesharp::driver
