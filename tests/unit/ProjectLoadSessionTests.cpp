#include "nativesharp/Compilation/ProjectGraphLoader.h"
#include "nativesharp/Compilation/ProjectLoader.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << contents;
}
}

int main()
{
    using nativesharp::compilation::ProjectGraphLoader;
    using nativesharp::compilation::ProjectLoader;
    using nativesharp::compilation::ProjectLoaderStatistics;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "tanda02-project-load-session";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);

    const std::filesystem::path child = root / "Child";
    writeFile(child / "Child.nsproj",
        "<Project><PropertyGroup><Name>Child</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"Child.ns\" /></ItemGroup></Project>");
    writeFile(child / "Child.ns", "namespace LoadSession { public class Dependency {} }");

    const std::filesystem::path project = root / "Root.nsproj";
    writeFile(project,
        "<Project><PropertyGroup><Name>Root</Name><OptimizationLevel>O0</OptimizationLevel>"
        "<CompilerJobs>2</CompilerJobs><OutputPath>bin/Root.lib</OutputPath></PropertyGroup>"
        "<ItemGroup><Compile Include=\"Root.ns\" />"
        "<ProjectReference Include=\"Child/Child.nsproj\" /></ItemGroup></Project>");
    writeFile(root / "Root.ns", "namespace LoadSession { public class RootType {} }");

    ProjectLoader loader;
    std::string error;
    const auto settings = loader.loadBuildSettings(project, error);
    ProjectGraphLoader graphLoader(2,
        [loader](const std::filesystem::path& path, std::string& loadError) {
            return loader.load(path, loadError);
        });
    const auto graph = graphLoader.load(project, error);
    const ProjectLoaderStatistics statistics = loader.statistics();

    tests.expect(settings.has_value() && settings->optimizationLevel == 0U &&
            settings->compilerJobs == 2U &&
            settings->outputPath == root / "bin/Root.lib",
        "project defaults are returned from the session descriptor");
    tests.expect(graph.has_value() && graph->projects().size() == 2U,
        "the same loader session expands the complete two-project graph (" + error + ")");
    tests.expect(statistics.descriptorFileReads == 2U &&
            statistics.descriptorCacheHits == 1U,
        "each nsproj is read once and the root descriptor is reused for graph loading");

    ProjectLoader independentSession;
    error.clear();
    const auto independentSettings = independentSession.loadBuildSettings(project, error);
    const ProjectLoaderStatistics independentStatistics = independentSession.statistics();
    tests.expect(independentSettings.has_value() &&
            independentStatistics.descriptorFileReads == 1U &&
            independentStatistics.descriptorCacheHits == 0U,
        "descriptor reuse is scoped to one loader session and does not become global state");

    std::filesystem::remove_all(root, filesystemError);
    return tests.result();
}
