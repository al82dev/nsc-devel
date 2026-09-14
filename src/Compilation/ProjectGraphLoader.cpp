#include "nativesharp/Compilation/ProjectGraphLoader.h"

#include "nativesharp/Compilation/ProjectLoader.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
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
    std::vector<std::filesystem::path> pathStack;
    std::vector<std::string> moduleStack;
    std::vector<Project> orderedProjects;
};

struct LoadedProject final
{
    std::filesystem::path path;
    std::optional<Project> project;
    std::string error;
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
    const std::map<std::string, Project>& projects,
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

    const auto loaded = projects.find(key);
    if (loaded == projects.end())
    {
        error = "internal error: discovered project was not loaded: '" +
            canonical.generic_string() + "'";
        return false;
    }
    const Project& project = loaded->second;
    state.visits.emplace(key, VisitState::Visiting);
    state.pathStack.push_back(project.projectFile());
    state.moduleStack.push_back(project.moduleName());

    std::vector<std::filesystem::path> references(
        project.projectReferences().begin(), project.projectReferences().end());
    std::ranges::sort(references, {}, [](const std::filesystem::path& path) {
        return pathKey(canonicalPath(path));
    });
    for (const std::filesystem::path& reference : references)
    {
        if (!visitProject(reference, projects, state, error))
        {
            return false;
        }
    }

    state.pathStack.pop_back();
    state.moduleStack.pop_back();
    state.visits[key] = VisitState::Complete;
    state.orderedProjects.push_back(project);
    return true;
}
bool normalizeTransitiveSourceOwnership(
    std::vector<Project>& projects,
    std::string& error)
{
    std::unordered_map<std::string, std::size_t> projectByPath;
    for (std::size_t ordinal = 0; ordinal < projects.size(); ++ordinal)
    {
        projectByPath.emplace(pathKey(canonicalPath(projects[ordinal].projectFile())), ordinal);
    }

    std::vector<std::unordered_set<std::string>> visibleSources;
    visibleSources.reserve(projects.size());
    std::vector<Project> normalized;
    normalized.reserve(projects.size());

    for (std::size_t ordinal = 0; ordinal < projects.size(); ++ordinal)
    {
        const Project& project = projects[ordinal];
        std::unordered_set<std::string> dependencySources;
        for (const std::filesystem::path& reference : project.projectReferences())
        {
            const auto dependency = projectByPath.find(pathKey(canonicalPath(reference)));
            if (dependency == projectByPath.end() || dependency->second >= ordinal)
            {
                error = "internal error: project source ownership requires topological references";
                return false;
            }
            dependencySources.insert(
                visibleSources[dependency->second].begin(),
                visibleSources[dependency->second].end());
        }

        ProjectSourceSet ownSources(project.sourceSet().projectDirectory());
        for (const ProjectSource& source : project.sourceSet().sources())
        {
            const std::string sourceKey = pathKey(canonicalPath(source.absolutePath));
            if (dependencySources.contains(sourceKey)) continue;
            if (!ownSources.add(source.absolutePath, error)) return false;
        }
        if (!ownSources.finalize(error, true)) return false;

        std::unordered_set<std::string> closure = std::move(dependencySources);
        for (const ProjectSource& source : ownSources.sources())
            closure.insert(pathKey(canonicalPath(source.absolutePath)));
        visibleSources.push_back(std::move(closure));

        normalized.emplace_back(
            project.projectFile(),
            project.moduleName(),
            project.outputKind(),
            std::move(ownSources),
            std::vector<std::filesystem::path>(
                project.projectReferences().begin(), project.projectReferences().end()),
            std::vector<LibraryReference>(
                project.libraryReferences().begin(), project.libraryReferences().end()));
    }

    projects = std::move(normalized);
    return true;
}

}

ProjectGraphLoader::ProjectGraphLoader(
    std::size_t workerCount,
    ProjectLoadFunction projectLoader)
    : workerCount_(std::max<std::size_t>(1, workerCount)),
      projectLoader_(std::move(projectLoader))
{
    if (!projectLoader_)
    {
        projectLoader_ = [](const std::filesystem::path& path, std::string& error) {
            return ProjectLoader{}.load(path, error);
        };
    }
}

std::optional<ProjectGraph> ProjectGraphLoader::load(
    const std::filesystem::path& rootProjectFile,
    std::string& error) const
{
    error.clear();
    const std::filesystem::path root = canonicalPath(rootProjectFile);
    std::set<std::string> discovered;
    discovered.insert(pathKey(root));
    std::vector<std::filesystem::path> frontier = {root};
    std::map<std::string, Project> projects;
    std::map<std::string, std::filesystem::path> moduleOwners;
    driver::CompilationWorkerPool pool(workerCount_);

    while (!frontier.empty())
    {
        std::ranges::sort(frontier, {}, [](const std::filesystem::path& path) {
            return pathKey(path);
        });
        std::vector<LoadedProject> loaded(frontier.size());
        for (std::size_t index = 0; index < frontier.size(); ++index)
        {
            loaded[index].path = frontier[index];
            pool.submit([&, index](std::size_t) {
                loaded[index].project = projectLoader_(loaded[index].path, loaded[index].error);
            });
        }
        try
        {
            pool.wait();
        }
        catch (const std::exception& exception)
        {
            error = "project reference loading failed: " + std::string(exception.what());
            return std::nullopt;
        }
        catch (...)
        {
            error = "project reference loading failed with a non-standard exception";
            return std::nullopt;
        }

        std::vector<std::filesystem::path> next;
        for (LoadedProject& entry : loaded)
        {
            if (!entry.project.has_value())
            {
                error = entry.error;
                return std::nullopt;
            }
            const std::string key = pathKey(entry.path);
            const std::string& moduleName = entry.project->moduleName();
            const auto owner = moduleOwners.find(moduleName);
            if (owner != moduleOwners.end() && pathKey(owner->second) != key)
            {
                error = "duplicate module name '" + moduleName + "' in projects '" +
                    owner->second.generic_string() + "' and '" +
                    entry.project->projectFile().generic_string() + "'";
                return std::nullopt;
            }
            moduleOwners[moduleName] = entry.project->projectFile();
            for (const std::filesystem::path& reference : entry.project->projectReferences())
            {
                const std::filesystem::path canonicalReference = canonicalPath(reference);
                if (discovered.insert(pathKey(canonicalReference)).second)
                {
                    next.push_back(canonicalReference);
                }
            }
            projects.emplace(key, std::move(*entry.project));
        }
        frontier = std::move(next);
    }

    GraphLoadState state;
    if (!visitProject(root, projects, state, error))
    {
        return std::nullopt;
    }

    if (state.orderedProjects.empty())
    {
        error = "project graph contains no projects";
        return std::nullopt;
    }
    if (!normalizeTransitiveSourceOwnership(state.orderedProjects, error))
    {
        return std::nullopt;
    }


    const std::filesystem::path canonicalRoot = state.orderedProjects.back().projectFile();
    return ProjectGraph(canonicalRoot, std::move(state.orderedProjects));
}
}
