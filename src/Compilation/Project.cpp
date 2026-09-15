#include "nativesharp/Compilation/Project.h"

#include <utility>

namespace nativesharp::compilation
{
Project::Project(
    std::filesystem::path projectFile,
    std::string moduleName,
    ProjectOutputKind outputKind,
    ProjectSourceSet sourceSet,
    std::vector<std::filesystem::path> projectReferences,
    std::vector<LibraryReference> libraryReferences,
    std::vector<SdkManifest> sdkReferences)
    : projectFile_(std::move(projectFile)),
      moduleName_(std::move(moduleName)),
      outputKind_(outputKind),
      sourceSet_(std::move(sourceSet)),
      projectReferences_(std::move(projectReferences)),
      libraryReferences_(std::move(libraryReferences)),
      sdkReferences_(std::move(sdkReferences))
{
}

const std::filesystem::path& Project::projectFile() const noexcept
{
    return projectFile_;
}

const std::string& Project::moduleName() const noexcept
{
    return moduleName_;
}

ProjectOutputKind Project::outputKind() const noexcept
{
    return outputKind_;
}

const ProjectSourceSet& Project::sourceSet() const noexcept
{
    return sourceSet_;
}

std::span<const std::filesystem::path> Project::projectReferences() const noexcept
{
    return projectReferences_;
}

std::span<const LibraryReference> Project::libraryReferences() const noexcept
{
    return libraryReferences_;
}

std::span<const SdkManifest> Project::sdkReferences() const noexcept
{
    return sdkReferences_;
}
}
