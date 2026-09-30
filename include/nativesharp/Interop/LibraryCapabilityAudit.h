#pragma once

#include "nativesharp/Interop/LibraryMetadata.h"

#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::interop
{
struct LibraryCapabilitySurface final
{
    std::string id;
    std::string name;
    std::vector<std::string> present;
    std::vector<std::string> missing;
};

std::vector<LibraryCapabilitySurface> auditLibraryCapabilitySurfaces(
    std::span<const LibraryMetadata> modules);
std::string libraryCapabilitySurfaceReport(std::span<const LibraryMetadata> modules);
bool auditLibraryCapabilityDirectory(
    const std::filesystem::path& root,
    std::string& report,
    std::string& error);
}
