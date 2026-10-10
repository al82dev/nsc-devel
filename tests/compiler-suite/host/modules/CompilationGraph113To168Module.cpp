#include "CompilationTestSupport.h"

#include <chrono>
#include <cctype>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace nsc::suite
{
namespace
{
using namespace compilation_support;

std::string foldedPath(const std::filesystem::path& path)
{
    std::string value = std::filesystem::weakly_canonical(path).generic_string();
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
    {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

ProjectGraphLoader::ProjectLoadFunction mapLoader(
    std::unordered_map<std::string, Project> projects,
    std::function<void(const std::filesystem::path&)> visit = {})
{
    return [projects = std::move(projects), visit = std::move(visit)](
               const std::filesystem::path& path, std::string& error) mutable -> std::optional<Project>
    {
        if (visit)
        {
            visit(path);
        }
        const auto found = projects.find(foldedPath(path));
        if (found == projects.end())
        {
            error = "fixture project is absent: '" + path.generic_string() + "'";
            return std::nullopt;
        }
        return found->second;
    };
}

void graphPathCycleContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("graph_cycle");
    const auto app = tree.write("App.nsproj", "fixture");
    const auto core = tree.write("Core.nsproj", "fixture");
    std::unordered_map<std::string, Project> projects;
    projects.emplace(foldedPath(app), makeProject(app, "App", variant == 3 ?
        std::vector<std::filesystem::path>{tree.root() / "Missing.nsproj"} :
        std::vector<std::filesystem::path>{core}));
    projects.emplace(foldedPath(core), makeProject(core, "Core", variant == 4 ?
        std::vector<std::filesystem::path>{app} : std::vector<std::filesystem::path>{}));
    std::string error;
    const auto graph = ProjectGraphLoader(1U, mapLoader(std::move(projects))).load(app, error);
    if (variant == 3)
    {
        context.expect(!graph.has_value() && contains(error, "fixture project is absent"),
            "CF-COMPILATION-015 missing canonical dependency preserves producer diagnostic");
        return;
    }
    if (variant == 4)
    {
        context.expect(!graph.has_value() && contains(error, "cycle") &&
                contains(error, "App") && contains(error, "Core"),
            "CF-COMPILATION-015 canonical cycle diagnostic names the closed chain");
        return;
    }
    context.expect(graph.has_value() && graph->rootProjectFile() == std::filesystem::weakly_canonical(app),
        "CF-COMPILATION-015 graph canonicalizes project paths before publishing identity");
}

void parallelDiscoveryContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("parallel_graph");
    const auto app = tree.write("App.nsproj", "fixture");
    const auto left = tree.write("Left.nsproj", "fixture");
    const auto right = tree.write("Right.nsproj", "fixture");
    std::unordered_map<std::string, Project> projects;
    projects.emplace(foldedPath(app), makeProject(app, "App", {right, left}));
    projects.emplace(foldedPath(left), makeProject(left, variant == 4 ? "Shared" : "Left"));
    projects.emplace(foldedPath(right), makeProject(right, variant == 4 ? "shared" : "Right"));
    std::atomic_size_t active{0U};
    std::atomic_size_t maximum{0U};
    const auto observe = [&](const std::filesystem::path& path)
    {
        if (path == app)
        {
            return;
        }
        const std::size_t current = active.fetch_add(1U) + 1U;
        std::size_t prior = maximum.load();
        while (prior < current && !maximum.compare_exchange_weak(prior, current)) {}
        if (variant == 2 || variant == 6 || variant == 8)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        active.fetch_sub(1U);
    };
    std::string error;
    const std::size_t workers = variant == 1 ? 0U : (variant == 8 ? 8U : 2U);
    const auto graph = ProjectGraphLoader(workers, mapLoader(std::move(projects), observe)).load(app, error);
    if (variant == 4)
    {
        context.expect(!graph.has_value() && contains(error, "duplicate module name"),
            "CF-COMPILATION-016 duplicate module ownership is rejected after parallel discovery");
        return;
    }
    context.expect(graph.has_value() && equalProjectOrder(*graph, {"Left", "Right", "App"}) &&
            (workers < 2U || maximum.load() >= 1U),
        "CF-COMPILATION-016 worker discovery preserves deterministic unique module publication");
}

void topologicalOrderContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("topology");
    const auto app = tree.write("App.nsproj", "fixture");
    const auto left = tree.write("Left.nsproj", "fixture");
    const auto right = tree.write("Right.nsproj", "fixture");
    const auto base = tree.write("Base.nsproj", "fixture");
    std::unordered_map<std::string, Project> projects;
    projects.emplace(foldedPath(app), makeProject(app, "App", {right, left}));
    projects.emplace(foldedPath(left), makeProject(left, "Left", {base}));
    projects.emplace(foldedPath(right), makeProject(right, "Right", {base}));
    projects.emplace(foldedPath(base), makeProject(base, "Base", variant == 4 ?
        std::vector<std::filesystem::path>{app} : std::vector<std::filesystem::path>{}));
    std::string error;
    const auto graph = ProjectGraphLoader(4U, mapLoader(std::move(projects))).load(app, error);
    if (variant == 4)
    {
        context.expect(!graph.has_value() && contains(error, "cycle"),
            "CF-COMPILATION-017 graph cycle is rejected rather than partially ordered");
        return;
    }
    context.expect(graph.has_value() && equalProjectOrder(*graph, {"Base", "Left", "Right", "App"}),
        "CF-COMPILATION-017 dependencies precede consumers and root is final");
}

void sourceOwnershipContract(TestContext& context, std::size_t)
{
    TemporaryTree tree("source_ownership");
    const auto shared = tree.write("Shared.ns", "fixture");
    const auto appFile = tree.write("App.nsproj", "fixture");
    const auto depFile = tree.write("Dep.nsproj", "fixture");
    auto sourceSet = [&](const std::filesystem::path& owner)
    {
        ProjectSourceSet sources(owner.parent_path());
        std::string error;
        sources.add(shared, error);
        sources.finalize(error);
        return sources;
    };
    Project dep(depFile, "Dep", ProjectOutputKind::Library, sourceSet(depFile), {}, {}, {});
    Project app(appFile, "App", ProjectOutputKind::Executable, sourceSet(appFile), {depFile}, {}, {});
    std::unordered_map<std::string, Project> projects;
    projects.emplace(foldedPath(depFile), std::move(dep));
    projects.emplace(foldedPath(appFile), std::move(app));
    std::string error;
    const auto graph = ProjectGraphLoader(2U, mapLoader(std::move(projects))).load(appFile, error);
    context.expect(graph.has_value() && graph->projects().size() == 2U &&
            graph->projects()[0].sourceSet().sources().size() == 1U &&
            graph->projects()[1].sourceSet().sources().empty(),
        "CF-COMPILATION-018 first dependency-first source owner wins and consumers are rebuilt without it");
}

void sourceArtifactPrecedenceContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("source_precedence");
    const auto coreFile = tree.write("Core.nsproj", "fixture");
    const auto appFile = tree.write("App.nsproj", "fixture");
    const std::string stem = variant == 4 ? "Other" : "Core";
    const LibraryReference binary{
        tree.root() / (stem + ".lib"), tree.root() / (stem + ".nsmeta"), {}};
    std::unordered_map<std::string, Project> projects;
    projects.emplace(foldedPath(coreFile), makeProject(coreFile, "Core"));
    projects.emplace(foldedPath(appFile), makeProject(appFile, "App", {coreFile}, {binary}));
    std::string error;
    const auto graph = ProjectGraphLoader(2U, mapLoader(std::move(projects))).load(appFile, error);
    const bool removed = graph.has_value() && graph->rootProject()->libraryReferences().empty();
    context.expect(graph.has_value() && removed == (variant != 4),
        "CF-COMPILATION-019 only exact library/metadata stem and source module identity receives source precedence");
}

void sourceAdmissionContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("source_admission");
    const auto source = tree.write("Models/Player.ns", "fixture");
    const auto duplicate = tree.root() / "Models" / "." / "Player.ns";
    const auto wrong = tree.write("Models/Player.txt", "fixture");
    const auto outside = std::filesystem::temp_directory_path() / "nsc_compilation_outside.ns";
    {
        std::ofstream stream(outside);
        stream << "fixture";
    }
    ProjectSourceSet sources(tree.root());
    std::string error;
    bool result = false;
    if (variant == 3)
    {
        result = !sources.add(tree.root() / "Missing.ns", error) && contains(error, "does not exist");
    }
    else if (variant == 4)
    {
        result = !sources.add(wrong, error) && contains(error, ".ns extension");
    }
    else if (variant == 5)
    {
        result = sources.add(source, error) && !sources.add(duplicate, error) && contains(error, "duplicate");
    }
    else if (variant == 6)
    {
        result = !sources.add(outside, error) && contains(error, "escapes");
    }
    else
    {
        result = sources.add(source, error);
    }
    std::error_code cleanup;
    std::filesystem::remove(outside, cleanup);
    context.expect(result, "CF-COMPILATION-020 source admission enforces existence, confinement, extension, and identity");
}

void sourceFinalizationContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("source_finalize");
    ProjectSourceSet sources(tree.root());
    std::string error;
    if (variant == 3)
    {
        context.expect(!sources.finalize(error) && contains(error, "at least one"),
            "CF-COMPILATION-021 empty source set requires explicit aggregate policy");
        return;
    }
    if (variant == 6)
    {
        context.expect(sources.finalize(error, true) && sources.sources().empty(),
            "CF-COMPILATION-021 aggregate node may explicitly finalize empty");
        return;
    }
    const std::size_t count = variant == 8 ? 512U : 3U;
    for (std::size_t reverse = count; reverse > 0U; --reverse)
    {
        const std::string name = "S" + std::to_string(reverse - 1U) + ".ns";
        const auto path = tree.write(name, "fixture");
        sources.add(path, error);
    }
    const bool first = sources.finalize(error);
    const bool second = sources.finalize(error);
    bool ordered = first && second && sources.isFinalized() && sources.sources().size() == count;
    for (std::size_t index = 0; ordered && index < count; ++index)
    {
        ordered = sources.sources()[index].ordinal == index;
    }
    if (variant == 4)
    {
        ordered = ordered && !sources.add(tree.root() / "S0.ns", error) && contains(error, "finalized");
    }
    context.expect(ordered, "CF-COMPILATION-021 finalization orders, numbers, freezes, and repeats idempotently");
}
}

void registerCompilationGraph113To168Module(TestRegistry& registry)
{
    addCompilationContract(registry, 15U, "unit", graphPathCycleContract);
    addCompilationContract(registry, 16U, "integration", parallelDiscoveryContract);
    addCompilationContract(registry, 17U, "unit", topologicalOrderContract);
    addCompilationContract(registry, 18U, "unit", sourceOwnershipContract);
    addCompilationContract(registry, 19U, "unit", sourceArtifactPrecedenceContract);
    addCompilationContract(registry, 20U, "integration", sourceAdmissionContract);
    addCompilationContract(registry, 21U, "unit", sourceFinalizationContract);
}
}
