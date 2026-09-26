#pragma once

#include "nativesharp/Compilation/Project.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace nativesharp::compilation
{
struct ProjectLoadOptions final
{
    // Relative LibraryReference Include, Metadata, and Runtime paths are
    // resolved from this directory instead of the directory that owns the
    // project file. Absolute paths keep their existing meaning.
    std::optional<std::filesystem::path> libraryReferenceRoot;
};

// Build defaults declared by the root .nsproj. The driver applies explicit
// command-line options after these values, so automation can keep stable
// defaults in the project without losing one-off overrides.
struct ProjectBuildSettings final
{
    std::optional<unsigned> optimizationLevel;
    std::optional<std::filesystem::path> outputPath;
    std::optional<std::filesystem::path> libraryReferenceRoot;
    std::optional<std::size_t> compilerJobs;
};

class ProjectLoader final
{
public:
    std::optional<ProjectBuildSettings> loadBuildSettings(
        const std::filesystem::path& projectFile,
        std::string& error) const;
    std::optional<Project> load(const std::filesystem::path& projectFile, std::string& error) const;
    std::optional<Project> load(
        const std::filesystem::path& projectFile,
        const ProjectLoadOptions& options,
        std::string& error) const;
};
}
