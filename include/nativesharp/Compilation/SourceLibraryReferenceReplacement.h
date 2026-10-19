#pragma once

#include <filesystem>

namespace nativesharp::compilation
{
struct SourceLibraryReferenceReplacement final
{
    std::filesystem::path ownerProjectFile;
    std::filesystem::path metadataPath;
    std::filesystem::path sourceProjectFile;
};
}
