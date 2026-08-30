#include "nativesharp/Compilation/ProjectGraphLoader.h"

#include "nativesharp/Compilation/ProjectLoader.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::compilation
{
namespace
{
enum class VisitState
{
    Visiting,
    Complete
};

struct GraphLoadState final
{
    std::unordered_map<std::string, VisitState> visits;
    std::unordered_map<std::string, std::filesystem::path> moduleOwners;
    std::vector<std::filesystem::path> pathStack;
    std::vector<std::string> moduleStack;
    std::vector<Project> orderedProjects;
};

std::string pathKey(const std::filesystem::path& path)
{
    std::string key = path.generic_string();
    std::transform(
        key.begin(),
        key.end(),
        key.begin(),
        [](unsigned char value)
        {
            return static_cast<char>(std::tolower(value));
        });
    return key;
}

std::filesystem::path canonicalPath(const std::filesystem::path& path)
{
    std::error_code errorCode;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, errorCode);
    if (errorCode)
    {
        return std::filesystem::absolute(path).lexically_normal();
    }
    return canonical;
}

std::string cycleMessage(const GraphLoadState& state, const std::filesystem::path& repeatedPath)
{
    const std::string repeatedKey = pathKey(repeatedPath);
    std::size_t begin = 0;
    while (begin < state.pathStack.size() && pathKey(state.pathStack[begin]) != repeatedKey)
    {
        ++begin;
    }

    std::string message = "project reference cycle: ";
    for (std::size_t index = begin; index < state.moduleStack.size(); ++index)
    {
        if (index != begin)
        {
            message += " -> ";
        }
        message += state.moduleStack[index];
    }
    if (begin < state.moduleStack.size())
    {
        message += " -> " + state.moduleStack[begin];
    }
    else
    {
        message += repeatedPath.generic_string();
    }
    return message;
}

bool visitProject(
    const std::filesystem::path& projectFile,
    const ProjectLoader& loader,
    GraphLoadState& state,
    std::string& error)
{
    const std::filesystem::path canonical = canonicalPath(projectFile);
    const std::string key = pathKey(canonical);
    const auto visit = state.visits.find(key);
    if (visit != state.visits.end())
    {
        if (visit->second == VisitState::Complete)
        {
            return true;
        }
        error = cycleMessage(state, canonical);
        return false;
    }

    std::string projectError;
    std::optional<Project> project = loader.load(canonical, projectError);
    if (!project.has_value())
    {
        error = projectError;
        return false;
    }

    const auto owner = state.moduleOwners.find(project->moduleName());
    if (owner != state.moduleOwners.end() && pathKey(owner->second) != key)
    {
        error = "duplicate module name '" + project->moduleName() + "' in projects '" +
            owner->second.generic_string() + "' and '" + project->projectFile().generic_string() + "'";
        return false;
    }
    state.moduleOwners[project->moduleName()] = project->projectFile();
    state.visits.emplace(key, VisitState::Visiting);
    state.pathStack.push_back(project->projectFile());
    state.moduleStack.push_back(project->moduleName());

    for (const std::filesystem::path& reference : project->projectReferences())
    {
        if (!visitProject(reference, loader, state, error))
        {
            return false;
        }
    }

    state.pathStack.pop_back();
    state.moduleStack.pop_back();
    state.visits[key] = VisitState::Complete;
    state.orderedProjects.push_back(std::move(*project));
    return true;
}
}

std::optional<ProjectGraph> ProjectGraphLoader::load(
    const std::filesystem::path& rootProjectFile,
    std::string& error) const
{
    error.clear();
    ProjectLoader loader;
    GraphLoadState state;
    if (!visitProject(rootProjectFile, loader, state, error))
    {
        return std::nullopt;
    }

    if (state.orderedProjects.empty())
    {
        error = "project graph contains no projects";
        return std::nullopt;
    }

    const std::filesystem::path canonicalRoot = state.orderedProjects.back().projectFile();
    return ProjectGraph(canonicalRoot, std::move(state.orderedProjects));
}
}
