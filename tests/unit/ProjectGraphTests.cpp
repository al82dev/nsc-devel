#include "nativesharp/Compilation/ProjectGraphDumper.h"
#include "nativesharp/Compilation/ProjectGraphLoader.h"
#include "nativesharp/Compilation/ProjectLoader.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nativesharp::compilation::ProjectGraphDumper;
using nativesharp::compilation::ProjectGraphLoader;
using nativesharp::compilation::PrecompiledProjectDescriptor;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void writeProject(
    const std::filesystem::path& directory,
    std::string_view fileName,
    std::string_view moduleName,
    std::string_view references = {})
{
    writeFile(directory / "Main.ns", "int Value() { return 1; }\n");
    std::string project =
        "<Project>\n"
        "  <PropertyGroup><Name>" + std::string(moduleName) + "</Name></PropertyGroup>\n";
    if (!references.empty())
    {
        project += "  <ItemGroup>\n";
        project += references;
        project += "  </ItemGroup>\n";
    }
    project += "</Project>\n";
    writeFile(directory / fileName, project);
}

void testDependencyOrderAndDiamond(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path shared = root / "Shared";
    const std::filesystem::path left = root / "Left";
    const std::filesystem::path right = root / "Right";
    const std::filesystem::path app = root / "App";

    writeProject(shared, "Shared.nsproj", "Shared");
    writeProject(
        left,
        "Left.nsproj",
        "Left",
        "    <ProjectReference Include=\"../Shared/Shared.nsproj\" />\n");
    writeProject(
        right,
        "Right.nsproj",
        "Right",
        "    <ProjectReference Include=\"../Shared/Shared.nsproj\" />\n");
    writeProject(
        app,
        "App.nsproj",
        "App",
        "    <ProjectReference Include=\"../Right/Right.nsproj\" />\n"
        "    <ProjectReference Include=\"../Left/Left.nsproj\" />\n");

    ProjectGraphLoader loader;
    std::string error;
    const auto graph = loader.load(app / "App.nsproj", error);
    tests.expect(graph.has_value(), "diamond project graph loads");
    if (!graph.has_value())
    {
        return;
    }

    tests.expect(graph->projects().size() == 4, "shared diamond dependency is loaded only once");
    tests.expect(graph->projects()[0].moduleName() == "Shared", "deepest dependency is ordered first");
    tests.expect(graph->projects()[1].moduleName() == "Left", "sorted left dependency follows shared");
    tests.expect(graph->projects()[2].moduleName() == "Right", "sorted right dependency follows left");
    tests.expect(graph->projects()[3].moduleName() == "App", "root project is ordered last");
    tests.expect(
        graph->rootProject() != nullptr && graph->rootProject()->moduleName() == "App",
        "root project accessor returns the graph root");

    std::ostringstream firstDump;
    std::ostringstream secondDump;
    ProjectGraphDumper dumper;
    dumper.dump(*graph, firstDump);
    dumper.dump(*graph, secondDump);
    tests.expect(firstDump.str() == secondDump.str(), "project graph dump is deterministic");
    tests.expect(firstDump.str().find("[0] Shared") != std::string::npos, "graph dump exposes dependency order");
    tests.expect(firstDump.str().find("[3] App (root)") != std::string::npos, "graph dump marks root project");
}

void testCycleDiagnostic(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path a = root / "CycleA";
    const std::filesystem::path b = root / "CycleB";
    const std::filesystem::path c = root / "CycleC";
    writeProject(
        a,
        "A.nsproj",
        "A",
        "    <ProjectReference Include=\"../CycleB/B.nsproj\" />\n");
    writeProject(
        b,
        "B.nsproj",
        "B",
        "    <ProjectReference Include=\"../CycleC/C.nsproj\" />\n");
    writeProject(
        c,
        "C.nsproj",
        "C",
        "    <ProjectReference Include=\"../CycleA/A.nsproj\" />\n");

    ProjectGraphLoader loader;
    std::string error;
    const auto graph = loader.load(a / "A.nsproj", error);
    tests.expect(!graph.has_value(), "cyclic project graph is rejected");
    tests.expect(
        error.find("project reference cycle: A -> B -> C -> A") != std::string::npos,
        "cycle diagnostic reports the deterministic module chain");
}

void testDuplicateModuleDiagnostic(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path first = root / "FirstShared";
    const std::filesystem::path second = root / "SecondShared";
    const std::filesystem::path app = root / "DuplicateApp";
    writeProject(first, "First.nsproj", "SharedModule");
    writeProject(second, "Second.nsproj", "SharedModule");
    writeProject(
        app,
        "App.nsproj",
        "DuplicateApp",
        "    <ProjectReference Include=\"../FirstShared/First.nsproj\" />\n"
        "    <ProjectReference Include=\"../SecondShared/Second.nsproj\" />\n");

    ProjectGraphLoader loader;
    std::string error;
    const auto graph = loader.load(app / "App.nsproj", error);
    tests.expect(!graph.has_value(), "two project files cannot own the same module identity");
    tests.expect(
        error.find("duplicate module name 'SharedModule'") != std::string::npos,
        "duplicate module diagnostic names the conflicting module");
}

void testMissingTransitiveReference(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path library = root / "BrokenLibrary";
    const std::filesystem::path app = root / "BrokenApp";
    writeProject(
        library,
        "BrokenLibrary.nsproj",
        "BrokenLibrary",
        "    <ProjectReference Include=\"../Missing/Missing.nsproj\" />\n");
    writeProject(
        app,
        "BrokenApp.nsproj",
        "BrokenApp",
        "    <ProjectReference Include=\"../BrokenLibrary/BrokenLibrary.nsproj\" />\n");

    ProjectGraphLoader loader;
    std::string error;
    const auto graph = loader.load(app / "BrokenApp.nsproj", error);
    tests.expect(!graph.has_value(), "missing transitive project reference rejects the graph");
    tests.expect(
        error.find("referenced project does not exist") != std::string::npos,
        "missing transitive project preserves ProjectLoader diagnostic");
}

void testIndependentReferencesLoadConcurrentlyAndRemainDeterministic(
    TestContext& tests,
    const std::filesystem::path& root)
{
    const std::filesystem::path left = root / "ParallelLeft";
    const std::filesystem::path right = root / "ParallelRight";
    const std::filesystem::path app = root / "ParallelApp";
    writeProject(left, "Left.nsproj", "ParallelLeft");
    writeProject(right, "Right.nsproj", "ParallelRight");
    writeProject(
        app,
        "App.nsproj",
        "ParallelApp",
        "    <ProjectReference Include=\"../ParallelRight/Right.nsproj\" />\n"
        "    <ProjectReference Include=\"../ParallelLeft/Left.nsproj\" />\n");

    std::mutex mutex;
    std::condition_variable bothStarted;
    std::size_t activeChildren = 0;
    std::size_t maximumActiveChildren = 0;
    bool releaseChildren = false;
    auto controlledLoad = [&](const std::filesystem::path& path, std::string& error) {
        if (path.parent_path() == left || path.parent_path() == right)
        {
            std::unique_lock lock(mutex);
            ++activeChildren;
            maximumActiveChildren = std::max(maximumActiveChildren, activeChildren);
            if (activeChildren == 2)
            {
                releaseChildren = true;
                bothStarted.notify_all();
            }
            if (!bothStarted.wait_for(lock, std::chrono::seconds(2), [&] { return releaseChildren; }))
            {
                releaseChildren = true;
                bothStarted.notify_all();
            }
            --activeChildren;
        }
        return nativesharp::compilation::ProjectLoader{}.load(path, error);
    };

    ProjectGraphLoader parallelLoader(2, controlledLoad);
    std::string error;
    const auto parallel = parallelLoader.load(app / "App.nsproj", error);
    tests.expect(parallel.has_value(), "independent project references load in parallel");
    tests.expect(maximumActiveChildren == 2,
        "two independent project references execute concurrently in the bounded pool");

    ProjectGraphLoader serialLoader(1);
    const auto serial = serialLoader.load(app / "App.nsproj", error);
    tests.expect(serial.has_value(), "the same graph loads with one worker");
    if (!parallel.has_value() || !serial.has_value()) return;
    std::ostringstream parallelDump;
    std::ostringstream serialDump;
    ProjectGraphDumper{}.dump(*parallel, parallelDump);
    ProjectGraphDumper{}.dump(*serial, serialDump);
    tests.expect(parallelDump.str() == serialDump.str(),
        "parallel discovery preserves the exact serial project graph order");
}

void testPrecompiledOrderIsCanonical(TestContext& tests, const std::filesystem::path& root)
{
    const auto app = root / "CanonicalPrecompiled";
    writeProject(app, "App.nsproj", "App");
    writeFile(app / "A.nsmeta", "a");
    writeFile(app / "Z.nsmeta", "z");
    ProjectGraphLoader loader;
    std::string error;
    auto first = loader.load(app / "App.nsproj", error);
    auto second = loader.load(app / "App.nsproj", error);
    tests.expect(first.has_value() && second.has_value(), "canonical precompiled fixture loads twice");
    if (!first.has_value() || !second.has_value()) return;

    const std::vector<PrecompiledProjectDescriptor> forward = {
        {app / "App.nsproj", app / "Z.nsmeta", "Zulu"},
        {app / "App.nsproj", app / "A.nsmeta", "Alpha"}};
    const std::vector<PrecompiledProjectDescriptor> reverse = {forward[1], forward[0]};
    tests.expect(first->addPrecompiledProjects(forward, error), "forward metadata descriptors expand");
    tests.expect(second->addPrecompiledProjects(reverse, error), "reverse metadata descriptors expand");
    std::ostringstream firstDump;
    std::ostringstream secondDump;
    ProjectGraphDumper{}.dump(*first, firstDump);
    ProjectGraphDumper{}.dump(*second, secondDump);
    tests.expect(firstDump.str() == secondDump.str(),
        "descriptor discovery order cannot change the physical compilation graph");
    tests.expect(first->projects()[0].moduleName() == "Alpha" &&
            first->projects()[1].moduleName() == "Zulu",
        "precompiled modules use canonical module order");
}

void testShadowedLibraryReferenceBecomesSourceEdge(TestContext& tests, const std::filesystem::path& root)
{
    const auto core = root / "SourcePrecedenceCore";
    const auto io = root / "SourcePrecedenceIo";
    const auto app = root / "SourcePrecedenceApp";
    writeProject(core, "Core.nsproj", "Core");
    writeFile(io / "Core.lib", "archive fixture");
    writeFile(io / "Core.nsmeta", "metadata fixture");
    writeProject(
        io,
        "Io.nsproj",
        "Io",
        "    <LibraryReference Include=\"Core.lib\" Metadata=\"Core.nsmeta\" />\n");
    writeProject(
        app,
        "App.nsproj",
        "App",
        "    <ProjectReference Include=\"../SourcePrecedenceCore/Core.nsproj\" />\n"
        "    <ProjectReference Include=\"../SourcePrecedenceIo/Io.nsproj\" />\n");

    ProjectGraphLoader loader;
    std::string error;
    auto graph = loader.load(app / "App.nsproj", error);
    tests.expect(graph.has_value(), "source/binary precedence fixture loads");
    if (!graph.has_value()) return;

    const auto ioProject = std::ranges::find_if(graph->projects(), [](const auto& project) {
        return project.moduleName() == "Io";
    });
    tests.expect(ioProject != graph->projects().end() && ioProject->libraryReferences().size() == 1,
        "fixture initially contains the binary edge");

    const std::vector<nativesharp::compilation::SourceLibraryReferenceReplacement> replacements = {{
        io / "Io.nsproj", io / "Core.nsmeta", core / "Core.nsproj"}};
    tests.expect(graph->replaceLibraryReferencesWithSourceProjects(replacements, error),
        "source precedence rewires the graph");
    const auto filteredIo = std::ranges::find_if(graph->projects(), [](const auto& project) {
        return project.moduleName() == "Io";
    });
    tests.expect(filteredIo != graph->projects().end() && filteredIo->libraryReferences().empty(),
        "source precedence removes the shadowed binary edge");
    tests.expect(filteredIo != graph->projects().end() && filteredIo->projectReferences().size() == 1 &&
            filteredIo->projectReferences().front().filename() == "Core.nsproj",
        "source precedence adds the source dependency edge");
    tests.expect(graph->projects().size() == 3 && graph->rootProject() != nullptr &&
            graph->rootProject()->moduleName() == "App",
        "source precedence preserves project order and root identity");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage23_project_graph";
    std::error_code errorCode;
    std::filesystem::remove_all(root, errorCode);
    std::filesystem::create_directories(root, errorCode);

    testDependencyOrderAndDiamond(tests, root);
    testIndependentReferencesLoadConcurrentlyAndRemainDeterministic(tests, root);
    testCycleDiagnostic(tests, root);
    testDuplicateModuleDiagnostic(tests, root);
    testMissingTransitiveReference(tests, root);
    testPrecompiledOrderIsCanonical(tests, root);
    testShadowedLibraryReferenceBecomesSourceEdge(tests, root);

    std::filesystem::remove_all(root, errorCode);
    return tests.result();
}
