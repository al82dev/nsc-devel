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
};
} // namespace nativesharp::driver
