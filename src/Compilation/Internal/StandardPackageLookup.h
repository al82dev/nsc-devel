#pragma once

#include "nativesharp/Compilation/ProjectLoadOptions.h"

#include <filesystem>
#include <optional>
#include <string>

namespace nativesharp::compilation
{
std::optional<std::filesystem::path> standardPackageArtifact(
    const std::filesystem::path& artifact, const ProjectLoadOptions& options, std::string& error);
}
