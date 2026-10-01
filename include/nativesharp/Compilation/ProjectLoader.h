#pragma once

#include "nativesharp/Compilation/Project.h"

#include <cstddef>
#include <filesystem>
#include <memory>
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

    // Environment-provided package roots are fallbacks, not project rewrites. A relative
    // LibraryReference that already exists beside its project keeps that local meaning; only a
    // missing project-relative artifact is searched below this directory.
    std::optional<std::filesystem::path> libraryReferenceFallbackRoot;
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

struct ProjectLoaderStatistics final
{
    std::size_t descriptorFileReads = 0;
    std::size_t descriptorCacheHits = 0;
};

class ProjectLoaderCache;

class ProjectLoader final
{
public:
    ProjectLoader();

    std::optional<ProjectBuildSettings> loadBuildSettings(
        const std::filesystem::path& projectFile,
        std::string& error) const;
    std::optional<Project> load(const std::filesystem::path& projectFile, std::string& error) const;
    std::optional<Project> load(
        const std::filesystem::path& projectFile,
        const ProjectLoadOptions& options,
        std::string& error) const;
    ProjectLoaderStatistics statistics() const;

private:
    std::shared_ptr<ProjectLoaderCache> cache_;
};
}
