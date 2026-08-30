#include "nativesharp/Compilation/ProjectGraph.h"

#include <utility>

namespace nativesharp::compilation
{
ProjectGraph::ProjectGraph(std::filesystem::path rootProjectFile, std::vector<Project> projects)
    : rootProjectFile_(std::move(rootProjectFile)),
      projects_(std::move(projects))
{
}

const std::filesystem::path& ProjectGraph::rootProjectFile() const noexcept
{
    return rootProjectFile_;
}

std::span<const Project> ProjectGraph::projects() const noexcept
{
    return projects_;
}

const Project* ProjectGraph::rootProject() const noexcept
{
    if (projects_.empty())
    {
        return nullptr;
    }
    return &projects_.back();
}
}
