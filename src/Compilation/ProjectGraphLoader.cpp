#include "nativesharp/Compilation/ProjectGraphLoader.h"

#include "nativesharp/Compilation/ProjectLoader.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Language/ModuleSemantics.h"

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
    std::vector<language::LanguageEdition> editionStack;
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

bool resolveImplicitLanguageEditions(
    std::map<std::string, Project>& projects,
    std::string& error)
{
    const Project* authority = nullptr;
    for (const auto& [path, project] : projects)
    {
        (void)path;
        if (!project.hasExplicitLanguageEdition()) continue;
        if (authority == nullptr)
        {
            authority = &project;
            continue;
        }
        if (authority->languageEdition() == project.languageEdition()) continue;

        error = "NS3001: project '" + authority->moduleName() +
            "' explicitly uses language edition " +
            std::string(language::languageEditionName(authority->languageEdition())) +
            " but project '" + project.moduleName() +
            "' explicitly uses language edition " +
            std::string(language::languageEditionName(project.languageEdition())) +
            "; mixed explicitly declared editions are not allowed inside one module graph";
        return false;
    }

    // Omitted <Language> is a compatibility request, not an explicit Language 2
    // boundary. Preserve the historical V2 default when the whole graph is
    // implicit; otherwise make every implicit node follow the graph's sole
    // explicit contract before cycle and edge validation.
    if (authority != nullptr)
    {
        const language::LanguageEdition inferred = authority->languageEdition();
        for (auto& [path, project] : projects)
        {
            (void)path;
            project.adoptInferredLanguageEdition(inferred);
        }
    }
    return true;
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

bool semanticBindingCycleAllowed(
    const GraphLoadState& state,
    const std::filesystem::path& repeatedPath) noexcept
{
    const std::string repeatedKey = pathKey(repeatedPath);
    std::size_t begin = 0;
    while (begin < state.pathStack.size() && pathKey(state.pathStack[begin]) != repeatedKey)
        ++begin;
    if (begin == state.pathStack.size()) return false;
    for (std::size_t index = begin; index < state.editionStack.size(); ++index)
        if (state.editionStack[index] != language::LanguageEdition::V3) return false;
    return true;
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
        // Edition 3 modules are semantic declaration spaces rather than an include order. A
        // binding cycle is therefore admissible here; recursive by-value layout remains a later
        // object-layout error once every declaration in the strongly connected component exists.
        if (semanticBindingCycleAllowed(state, canonical)) return true;
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
    state.editionStack.push_back(project.languageEdition());

    std::vector<std::filesystem::path> references(
        project.projectReferences().begin(), project.projectReferences().end());
    std::ranges::sort(references, {}, [](const std::filesystem::path& path) {
        return pathKey(canonicalPath(path));
    });
    for (const std::filesystem::path& reference : references)
    {
        const auto dependency = projects.find(pathKey(canonicalPath(reference)));
        if (dependency == projects.end())
        {
            error = "internal error: project reference was not loaded: '" +
                reference.generic_string() + "'";
            return false;
        }
        if (dependency->second.languageEdition() != project.languageEdition())
        {
            error = "NS3001: project '" + project.moduleName() + "' uses language edition " +
                std::string(language::languageEditionName(project.languageEdition())) +
                " but imports project '" + dependency->second.moduleName() +
                "' using language edition " +
                std::string(language::languageEditionName(dependency->second.languageEdition())) +
                "; mixed editions are not allowed inside one module graph";
            return false;
        }
        if (!visitProject(reference, projects, state, error))
        {
            return false;
        }
    }

    state.pathStack.pop_back();
    state.moduleStack.pop_back();
    state.editionStack.pop_back();
    state.visits[key] = VisitState::Complete;
    state.orderedProjects.push_back(project);
    return true;
}
bool normalizeTransitiveSourceOwnership(
    std::vector<Project>& projects,
    std::string& error)
{
    // A source file has one semantic module owner even when broad globs in several referenced
    // projects discover it. Dependency-first graph order preserves the historical owner for
    // acyclic graphs; strongly connected edition-3 components use their deterministic DFS order.
    std::unordered_map<std::string, std::size_t> sourceOwner;
    for (std::size_t ordinal = 0; ordinal < projects.size(); ++ordinal)
    {
        for (const ProjectSource& source : projects[ordinal].sourceSet().sources())
            sourceOwner.try_emplace(pathKey(canonicalPath(source.absolutePath)), ordinal);
    }

    std::vector<Project> normalized;
    normalized.reserve(projects.size());
    for (std::size_t ordinal = 0; ordinal < projects.size(); ++ordinal)
    {
        const Project& project = projects[ordinal];
        ProjectSourceSet ownSources(project.sourceSet().projectDirectory());
        for (const ProjectSource& source : project.sourceSet().sources())
        {
            const std::string sourceKey = pathKey(canonicalPath(source.absolutePath));
            const auto owner = sourceOwner.find(sourceKey);
            if (owner == sourceOwner.end() || owner->second != ordinal) continue;
            if (!ownSources.add(source.absolutePath, error)) return false;
        }
        if (!ownSources.finalize(error, true)) return false;
        normalized.emplace_back(
            project.projectFile(),
            project.moduleName(),
            project.outputKind(),
            std::move(ownSources),
            std::vector<std::filesystem::path>(
                project.projectReferences().begin(), project.projectReferences().end()),
            std::vector<LibraryReference>(
                project.libraryReferences().begin(), project.libraryReferences().end()),
            std::vector<SdkManifest>(
                project.sdkReferences().begin(), project.sdkReferences().end()),
            project.languageEdition(), project.panicProfile(),
            std::string(project.panicHandler()), std::string(project.packageName()),
            std::vector<std::string>(
                project.friendModules().begin(), project.friendModules().end()),
            project.hasExplicitLanguageEdition());
    }

    projects = std::move(normalized);
    return true;
}

void preferExactSourceModuleArtifacts(
    std::map<std::string, Project>& projects,
    const std::map<std::string, std::filesystem::path>& moduleOwners)
{
    std::map<std::string, Project> rewired;
    for (const auto& [key, project] : projects)
    {
        std::vector<std::filesystem::path> projectReferences(
            project.projectReferences().begin(), project.projectReferences().end());
        std::vector<LibraryReference> libraryReferences;
        libraryReferences.reserve(project.libraryReferences().size());
        for (const LibraryReference& reference : project.libraryReferences())
        {
            const std::string libraryStem = reference.libraryPath.stem().string();
            const std::string metadataStem = reference.metadataPath.stem().string();
            const auto source = libraryStem == metadataStem
                ? moduleOwners.find(
                    language::canonicalWindowsModuleIdentity({}, libraryStem))
                : moduleOwners.end();
            if (source == moduleOwners.end() ||
                pathKey(canonicalPath(source->second)) ==
                    pathKey(canonicalPath(project.projectFile())))
            {
                libraryReferences.push_back(reference);
                continue;
            }

            const std::string sourceKey = pathKey(canonicalPath(source->second));
            const bool alreadyReferenced = std::ranges::any_of(
                projectReferences, [&](const std::filesystem::path& path) {
                    return pathKey(canonicalPath(path)) == sourceKey;
                });
            if (!alreadyReferenced) projectReferences.push_back(source->second);
        }
        std::ranges::sort(projectReferences, {}, [](const std::filesystem::path& path) {
            return pathKey(canonicalPath(path));
        });
        std::ranges::sort(libraryReferences, {}, [](const LibraryReference& reference) {
            return pathKey(canonicalPath(reference.libraryPath));
        });
        rewired.emplace(
            key,
            Project(
                project.projectFile(), project.moduleName(), project.outputKind(),
                project.sourceSet(), std::move(projectReferences),
                std::move(libraryReferences),
                std::vector<SdkManifest>(
                    project.sdkReferences().begin(), project.sdkReferences().end()),
                project.languageEdition(), project.panicProfile(),
                std::string(project.panicHandler()), std::string(project.packageName()),
                std::vector<std::string>(
                    project.friendModules().begin(), project.friendModules().end()),
                project.hasExplicitLanguageEdition()));
    }
    projects = std::move(rewired);
}

}

ProjectGraphLoader::ProjectGraphLoader(
    std::size_t workerCount,
    ProjectLoadFunction projectLoader,
    ProjectLoadOptions options)
    : workerCount_(std::max<std::size_t>(1, workerCount)),
      projectLoader_(std::move(projectLoader))
{
    if (!projectLoader_)
    {
        projectLoader_ = [options = std::move(options)](
            const std::filesystem::path& path, std::string& error) {
            return ProjectLoader{}.load(path, options, error);
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
            const std::string moduleKey =
                language::canonicalWindowsModuleIdentity({}, moduleName);
            const auto owner = moduleOwners.find(moduleKey);
            if (owner != moduleOwners.end() && pathKey(owner->second) != key)
            {
                error = "duplicate module name '" + moduleName + "' in projects '" +
                    owner->second.generic_string() + "' and '" +
                    entry.project->projectFile().generic_string() + "'";
                return std::nullopt;
            }
            moduleOwners[moduleKey] = entry.project->projectFile();
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

    // Direct frontend clients do not run the Driver's NSMETA closure pass. Preserve source
    // precedence for the unambiguous convention where both artifact stems exactly equal a
    // source module already present in the graph. Non-matching package names remain untouched
    // for the Driver's authoritative metadata-identity resolution.
    preferExactSourceModuleArtifacts(projects, moduleOwners);

    if (!resolveImplicitLanguageEditions(projects, error))
    {
        return std::nullopt;
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
