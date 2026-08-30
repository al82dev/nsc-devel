#pragma once

#include "nativesharp/Compilation/ProjectSourceSet.h"

#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::compilation
{
enum class ProjectOutputKind
{
    Executable,
    Library,
    SharedLibrary
};

class Project final
{
public:
    Project(
        std::filesystem::path projectFile,
        std::string moduleName,
        ProjectOutputKind outputKind,
        ProjectSourceSet sourceSet,
        std::vector<std::filesystem::path> projectReferences);

    const std::filesystem::path& projectFile() const noexcept;
    const std::string& moduleName() const noexcept;
    ProjectOutputKind outputKind() const noexcept;
    const ProjectSourceSet& sourceSet() const noexcept;
    std::span<const std::filesystem::path> projectReferences() const noexcept;

private:
    std::filesystem::path projectFile_;
    std::string moduleName_;
    ProjectOutputKind outputKind_;
    ProjectSourceSet sourceSet_;
    std::vector<std::filesystem::path> projectReferences_;
};
}
