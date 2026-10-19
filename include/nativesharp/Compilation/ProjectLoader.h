#pragma once

#include "nativesharp/Compilation/ProjectBuildSettings.h"
#include "nativesharp/Compilation/ProjectLoadOptions.h"
#include "nativesharp/Compilation/Project.h"
#include "nativesharp/Compilation/ProjectLoaderStatistics.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace nativesharp::compilation
{
inline constexpr std::size_t MaximumGenericInstantiationDepth = 4096;
inline constexpr std::size_t MaximumGenericInstantiationCount = 1'048'576;

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
