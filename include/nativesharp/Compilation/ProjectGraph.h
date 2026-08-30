#pragma once

#include "nativesharp/Compilation/Project.h"

#include <filesystem>
#include <span>
#include <vector>

namespace nativesharp::compilation
{
class ProjectGraph final
{
public:
    ProjectGraph(std::filesystem::path rootProjectFile, std::vector<Project> projects);

    const std::filesystem::path& rootProjectFile() const noexcept;
    std::span<const Project> projects() const noexcept;
    const Project* rootProject() const noexcept;

private:
    std::filesystem::path rootProjectFile_;
    std::vector<Project> projects_;
};
}
