#include "CompilationTestSupport.h"

#include <cstdlib>

namespace nsc::suite
{
namespace
{
using namespace compilation_support;

std::optional<Project> loadDocument(
    TemporaryTree& tree,
    std::string_view document,
    std::string& error,
    const ProjectLoadOptions& options = {},
    std::string_view name = "App.nsproj",
    bool writeSource = true)
{
    if (writeSource)
    {
        tree.write("Main.ns", "int main() { return 0; }\n");
    }
    const auto project = tree.write(std::string(name), document);
    return loadProject(project, error, options);
}

void decimalBudgetContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("budget");
    const std::string value = variant == 1 ? "1" :
        variant == 2 ? "64" :
        variant == 3 ? "" :
        variant == 4 ? "12x" :
        variant == 5 ? "0" :
        variant == 6 ? std::to_string(MaximumGenericInstantiationDepth) :
        variant == 7 ? "0007" : std::to_string(MaximumGenericInstantiationDepth + 1U);
    std::string error;
    const auto settings = ProjectLoader{}.loadBuildSettings(
        tree.write("App.nsproj", projectDocument(
            "<GenericInstantiationDepth>" + value + "</GenericInstantiationDepth>")), error);
    const bool expected = variant == 1 || variant == 2 || variant == 6 || variant == 7;
    context.expect(settings.has_value() == expected && (settings.has_value() ||
            contains(error, "GenericInstantiationDepth")),
        "CF-COMPILATION-028 positive decimal budgets accept only bounded complete decimals");
}

void projectPropertyContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("properties");
    std::string properties;
    if (variant == 2)
    {
        properties = "<Name>Game</Name><Package>Pkg</Package><OutputType>SharedLibrary</OutputType>"
            "<OptimizationLevel>O2</OptimizationLevel><Language>999</Language>"
            "<PanicProfile>abort</PanicProfile><PanicHandler>Pkg.Fail</PanicHandler>";
    }
    else if (variant == 3) properties = "<Name></Name>";
    else if (variant == 4) properties = "<OutputType>Object</OutputType>";
    else if (variant == 5) properties = "<Name>A</Name><Name>B</Name>";
    else if (variant == 6) properties = "<Language>2</Language>";
    else if (variant == 8) properties = "<CompilerJobs>1024</CompilerJobs>";
    std::string error;
    const auto project = loadDocument(tree, projectDocument(properties), error);
    const bool expected = variant != 3 && variant != 4 && variant != 5;
    bool exact = project.has_value() == expected;
    if (project.has_value() && variant == 2)
    {
        exact = project->moduleName() == "Game" && project->packageName() == "Pkg" &&
            project->outputKind() == ProjectOutputKind::SharedLibrary;
    }
    context.expect(exact, "CF-COMPILATION-029 property schema applies defaults and treats Language as neutral input");
}

void compileItemContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("compile_item");
    tree.write("Keep.ns", "fixture");
    tree.write("Drop.ns", "fixture");
    std::string item = variant == 1 ? "<Compile Include=\"Keep.ns\"/>" :
        variant == 2 ? "<Compile Include=\"*.ns\" Exclude=\"Drop.ns\"/>" :
        variant == 3 ? "<Compile/>" :
        variant == 4 ? "<Compile Include=\"*.ns\" Remove=\"Drop.ns\"/>" :
        variant == 5 ? "<Compile Include=\"Keep.ns;;Drop.ns\"/>" :
        variant == 6 ? "<Compile Include=\"*.ns\"/><Compile Remove=\"Drop.ns\"/>" :
        variant == 7 ? "<Compile Include=\"Keep.ns;Drop.ns\"/>" :
            "<Compile Include=\"**/*.ns\"/>";
    std::string error;
    const auto project = loadDocument(tree, projectDocument({}, item), error, {}, "App.nsproj", false);
    const bool expected = variant == 1 || variant == 2 || variant == 6 || variant == 7 || variant == 8;
    bool exact = project.has_value() == expected;
    if (project.has_value() && variant == 2)
    {
        exact = project->sourceSet().sources().size() == 1U &&
            project->sourceSet().sources().front().projectRelativePath == "Keep.ns";
    }
    context.expect(exact, "CF-COMPILATION-030 Compile enforces Include/Exclude/Remove schema and glob lists");
}

void projectReferenceSchemaContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("project_ref_schema");
    tree.write("Dep.ns", "fixture");
    tree.write("Dep.nsproj", projectDocument("<Name>Dep</Name>"));
    const std::string item = variant == 1 ? "<ProjectReference Include=\"Dep.nsproj\"/>" :
        variant == 2 ? "<ProjectReference Include=\"./Dep.nsproj\"/>" :
        variant == 3 ? "<ProjectReference/>" :
        variant == 4 ? "<ProjectReference Include=\"Dep.nsproj;Other.nsproj\"/>" :
        variant == 5 ? "<ProjectReference Include=\"Dep.nsproj\" Extra=\"x\"/>" :
        variant == 6 ? "<ProjectReference Include=\"DEP.nsproj\"/>" :
        variant == 7 ? "<ProjectReference Include=\"Dep.nsproj\"/>" :
            "<ProjectReference Include=\"Dep.nsproj\"/>";
    std::string error;
    const auto project = loadDocument(tree, projectDocument({}, item), error);
    const bool expected = variant == 1 || variant == 2 || variant == 6 ||
        variant == 7 || variant == 8;
    context.expect(project.has_value() == expected,
        "CF-COMPILATION-031 ProjectReference is closed, non-empty, and accepts one path per item");
}

void libraryReferenceSchemaContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("library_ref_schema");
    tree.write("Core.lib", "archive");
    tree.write("Core.nsmeta", "metadata");
    tree.write("Core.dll", "runtime");
    const std::string item = variant == 1 ? "<LibraryReference Include=\"Core.lib\"/>" :
        variant == 2 ? "<LibraryReference Include=\"Core.lib\" Metadata=\"Core.nsmeta\" Runtime=\"Core.dll\"/>" :
        variant == 3 ? "<LibraryReference/>" :
        variant == 4 ? "<LibraryReference Include=\"Core.lib;Other.lib\"/>" :
        variant == 5 ? "<LibraryReference Include=\"Core.lib\" Metadata=\"\"/>" :
        variant == 6 ? "<LibraryReference Include=\"Core.lib\" Runtime=\"Core.dll\"/>" :
        variant == 7 ? "<LibraryReference Include=\"Core.lib\" Metadata=\"Core.nsmeta\"/>" :
            "<LibraryReference Include=\"Core.lib\"/>";
    std::string error;
    const auto project = loadDocument(tree, projectDocument({}, item), error);
    const bool expected = variant == 1 || variant == 2 || variant == 6 || variant == 7 || variant == 8;
    bool exact = project.has_value() == expected;
    if (project.has_value())
    {
        exact = project->libraryReferences().size() == 1U &&
            project->libraryReferences().front().metadataPath.filename() == "Core.nsmeta";
    }
    context.expect(exact, "CF-COMPILATION-032 LibraryReference requires one .lib and mandatory metadata sidecar");
}

void sdkFriendSchemaContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("sdk_friend_schema");
    tree.write("Windows.nssdk", sdkDocument());
    const std::string items = variant == 1 ? "<SdkReference Include=\"Windows.nssdk\"/>" :
        variant == 2 ? "<SdkReference Include=\"Windows.nssdk\"/><Friend Include=\"Editor\"/>" :
        variant == 3 ? "<SdkReference/>" :
        variant == 4 ? "<Friend Include=\"bad-name\"/>" :
        variant == 5 ? "<Friend Include=\"Editor\"/><Friend Include=\"editor\"/>" :
        variant == 6 ? "<Friend Include=\"Editor\"/>" :
        variant == 7 ? "<SdkReference Include=\"Windows.nssdk\"/>" :
            "<SdkReference Include=\"Windows.nssdk\"/><Friend Include=\"Editor\"/>";
    std::string error;
    const auto project = loadDocument(tree, projectDocument({}, items), error);
    const bool expected = variant == 1 || variant == 2 || variant == 6 || variant == 7 || variant == 8;
    bool exact = project.has_value() == expected;
    if (project.has_value() && variant == 2)
    {
        exact = project->sdkReferences().size() == 1U && project->friendModules().size() == 1U;
    }
    context.expect(exact, "CF-COMPILATION-033 SDK and Friend items enforce closed single-identity schemas");
}

void projectRootSchemaContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("root_schema");
    const std::string source = variant == 1 ? "<Project/>" :
        variant == 2 ? projectDocument("<Name>App</Name>", "<Friend Include=\"Editor\"/>") :
        variant == 3 ? "<Other/>" :
        variant == 4 ? "<Project Extra=\"x\"/>" :
        variant == 5 ? "<Project>text</Project>" :
        variant == 6 ? "<Project><Unknown/></Project>" :
        variant == 7 ? "<Project><PropertyGroup/><ItemGroup/></Project>" :
            "<Project>" + std::string(64U, ' ') + "</Project>";
    std::string error;
    const auto project = loadDocument(tree, source, error);
    const bool expected = variant == 1 || variant == 2 || variant == 7 || variant == 8;
    context.expect(project.has_value() == expected && (project.has_value() || !error.empty()),
        "CF-COMPILATION-034 Project root accepts only schema-defined groups without attributes or text");
}

void projectFileContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("project_file");
    std::string error;
    std::filesystem::path path;
    if (variant == 3)
    {
        path = tree.root() / "Missing.nsproj";
    }
    else if (variant == 4)
    {
        path = tree.write("App.xml", "<Project/>");
    }
    else if (variant == 5)
    {
        path = tree.write("App.nsproj", std::string(4U * 1024U * 1024U + 1U, 'x'));
    }
    else
    {
        tree.write("Main.ns", "fixture");
        path = tree.write("App.nsproj", "<Project/>");
    }
    const auto project = loadProject(path, error);
    const bool expected = variant != 3 && variant != 4 && variant != 5;
    context.expect(project.has_value() == expected &&
            (project.has_value() ? project->projectFile() == std::filesystem::weakly_canonical(path) : !error.empty()),
        "CF-COMPILATION-035 project file canonicalization, extension, size, and exact read are enforced");
}

void sourceDiscoveryContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("source_discovery");
    tree.write("Root.ns", "fixture");
    tree.write("src/Keep.ns", "fixture");
    tree.write("src/Drop.ns", "fixture");
    tree.write("bin/Generated.ns", "fixture");
    tree.write("obj/Generated.ns", "fixture");
    tree.write("notes.txt", "fixture");
    std::string items;
    if (variant == 2) items = "<Compile Include=\"src/*.ns\" Exclude=\"src/Drop.ns\"/>";
    else if (variant == 3) items = "<Compile Include=\"missing/*.ns\"/>";
    else if (variant == 4) items = "<Compile Include=\"notes.txt\"/>";
    else if (variant == 6) items = "<Compile Include=\"**/*.ns\"/><Compile Remove=\"Root.ns\"/>";
    std::string error;
    const auto project = loadDocument(tree, projectDocument({}, items), error, {}, "App.nsproj", false);
    const bool expected = variant != 3 && variant != 4;
    bool exact = project.has_value() == expected;
    if (project.has_value() && variant == 1)
    {
        exact = project->sourceSet().sources().size() == 3U;
    }
    context.expect(exact, "CF-COMPILATION-036 discovery applies default globs, reserved directories, and filters");
}

void buildSettingsContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("build_settings");
    std::string properties = variant == 1 ? "" :
        "<OptimizationLevel>O3</OptimizationLevel><OutputPath>bin/App.exe</OutputPath>"
        "<LibraryReferenceRoot>lib</LibraryReferenceRoot><CompilerJobs>12</CompilerJobs>"
        "<GenericInstantiationDepth>100</GenericInstantiationDepth>"
        "<GenericInstantiationLimit>1000</GenericInstantiationLimit>";
    if (variant == 3) properties = "<CompilerJobs>0</CompilerJobs>";
    if (variant == 4) properties = "<OptimizationLevel>Ox</OptimizationLevel>";
    if (variant == 5) properties = "<CompilerJobs>1025</CompilerJobs>";
    const auto path = tree.write("App.nsproj", projectDocument(properties));
    std::string error;
    const auto settings = ProjectLoader{}.loadBuildSettings(path, error);
    const bool expected = variant != 3 && variant != 4 && variant != 5;
    bool exact = settings.has_value() == expected;
    if (settings.has_value() && variant == 2)
    {
        exact = settings->optimizationLevel == 3U && settings->compilerJobs == 12U &&
            settings->outputPath == tree.root() / "bin/App.exe" &&
            settings->libraryReferenceRoot == tree.root() / "lib";
    }
    context.expect(exact, "CF-COMPILATION-037 build settings extract bounds and resolve paths from project directory");
}

void projectReferenceResolutionContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("project_ref_resolution");
    tree.write("deps/A.nsproj", "<Project/>");
    tree.write("deps/B.nsproj", "<Project/>");
    std::string items = "<ProjectReference Include=\"deps/B.nsproj\"/>"
        "<ProjectReference Include=\"deps/A.nsproj\"/>";
    if (variant == 3) items = "<ProjectReference Include=\"deps/Missing.nsproj\"/>";
    if (variant == 4) items = "<ProjectReference Include=\"deps/A.txt\"/>";
    if (variant == 5) items = "<ProjectReference Include=\"deps/A.nsproj\"/>"
        "<ProjectReference Include=\"deps/./A.nsproj\"/>";
    std::string error;
    const auto project = loadDocument(tree, projectDocument({}, items), error);
    const bool expected = variant != 3 && variant != 4 && variant != 5;
    bool exact = project.has_value() == expected;
    if (project.has_value())
    {
        exact = project->projectReferences().size() == 2U &&
            project->projectReferences()[0].filename() == "A.nsproj";
    }
    context.expect(exact, "CF-COMPILATION-038 project references resolve, canonicalize, deduplicate, and sort");
}

void libraryRootContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("library_root");
    const auto explicitRoot = tree.root() / "explicit";
    const auto projectRoot = tree.root() / "project-lib";
    const auto fallbackRoot = tree.root() / "fallback";
    const auto selected = variant == 1 ? tree.root() :
        variant == 2 ? explicitRoot : variant == 6 ? fallbackRoot : projectRoot;
    std::filesystem::create_directories(selected);
    tree.write(std::filesystem::relative(selected / "Core.lib", tree.root()), "archive");
    tree.write(std::filesystem::relative(selected / "Core.nsmeta", tree.root()), "metadata");
    std::string properties;
    ProjectLoadOptions options;
    if (variant == 2) options.libraryReferenceRoot = explicitRoot;
    else if (variant == 6) options.libraryReferenceFallbackRoot = fallbackRoot;
    else if (variant != 1) properties = "<LibraryReferenceRoot>project-lib</LibraryReferenceRoot>";
    if (variant == 3) std::filesystem::remove(selected / "Core.lib");
    if (variant == 4) std::filesystem::remove(selected / "Core.nsmeta");
    const std::string item = "<LibraryReference Include=\"Core.lib\"/>";
    std::string error;
    const auto project = loadDocument(tree, projectDocument(properties, item), error, options);
    const bool expected = variant != 3 && variant != 4;
    context.expect(project.has_value() == expected,
        "CF-COMPILATION-039 explicit, project, local, and fallback roots preserve precedence and sidecars");
}

void sdkProjectLoadContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("sdk_project_load");
    std::string body = "<Libraries><Library Name=\"user32\" Path=\"lib/user32.lib\"/></Libraries>";
    if (variant == 4) body = "<Libraries><Library Name=\"bad-name\" Path=\"../bad.lib\"/></Libraries>";
    tree.write("Windows.nssdk", sdkDocument(body));
    if (variant != 3)
    {
        tree.write("lib/user32.lib", "archive");
    }
    const std::string items = variant == 5 ?
        "<SdkReference Include=\"Windows.nssdk\"/><SdkReference Include=\"./Windows.nssdk\"/>" :
        "<SdkReference Include=\"Windows.nssdk\"/>";
    std::string error;
    const auto project = loadDocument(tree, projectDocument({}, items), error);
    const bool expected = variant != 3 && variant != 4 && variant != 5;
    bool exact = project.has_value() == expected;
    if (project.has_value())
    {
        exact = project->sdkReferences().size() == 1U &&
            project->sdkReferences().front().libraries.size() == 1U;
    }
    context.expect(exact, "CF-COMPILATION-040 SDK loading validates dependencies, identity, deduplication, and order");
}

void descriptorCacheContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("descriptor_cache");
    tree.write("Main.ns", "fixture");
    const auto path = tree.write("App.nsproj", "<Project/>");
    ProjectLoader loader;
    std::string error;
    const auto first = loader.load(path, error);
    const auto second = loader.load(variant == 6 ? tree.root() / "." / "App.nsproj" : path, error);
    const ProjectLoaderStatistics statistics = loader.statistics();
    context.expect(first.has_value() && second.has_value() && statistics.descriptorFileReads == 1U &&
            statistics.descriptorCacheHits == 1U,
        "CF-COMPILATION-041 descriptor cache shares canonical reads and reports exact hits");
}

void projectMaterializationContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("materialization");
    if (variant != 3)
    {
        tree.write("Main.ns", "fixture");
    }
    if (variant == 6)
    {
        tree.write("Dep.nsproj", "<Project/>");
    }
    const std::string properties = variant == 2 ?
        "<Name>Game</Name><Package>Pkg</Package><OutputType>Library</OutputType>" : "";
    const std::string items = variant == 6 ? "<ProjectReference Include=\"Dep.nsproj\"/>" : "";
    std::string error;
    const auto project = loadDocument(
        tree, projectDocument(properties, items), error, {}, "App.nsproj", false);
    const bool expected = variant != 3;
    bool exact = project.has_value() == expected;
    if (project.has_value() && variant == 1)
    {
        exact = project->moduleName() == "App" && project->packageName() == "App" &&
            project->outputKind() == ProjectOutputKind::Executable;
    }
    if (project.has_value() && variant == 6)
    {
        exact = project->sourceSet().sources().empty() && project->projectReferences().size() == 1U;
    }
    context.expect(exact, "CF-COMPILATION-042 materialization applies defaults and permits only referenced aggregate nodes empty");
}
}

void registerCompilationProject217To336Module(TestRegistry& registry)
{
    addCompilationContract(registry, 28U, "unit", decimalBudgetContract);
    addCompilationContract(registry, 29U, "unit", projectPropertyContract);
    addCompilationContract(registry, 30U, "unit", compileItemContract);
    addCompilationContract(registry, 31U, "unit", projectReferenceSchemaContract);
    addCompilationContract(registry, 32U, "unit", libraryReferenceSchemaContract);
    addCompilationContract(registry, 33U, "unit", sdkFriendSchemaContract);
    addCompilationContract(registry, 34U, "unit", projectRootSchemaContract);
    addCompilationContract(registry, 35U, "integration", projectFileContract);
    addCompilationContract(registry, 36U, "integration", sourceDiscoveryContract);
    addCompilationContract(registry, 37U, "integration", buildSettingsContract);
    addCompilationContract(registry, 38U, "integration", projectReferenceResolutionContract);
    addCompilationContract(registry, 39U, "integration", libraryRootContract);
    addCompilationContract(registry, 40U, "integration", sdkProjectLoadContract);
    addCompilationContract(registry, 41U, "integration", descriptorCacheContract);
    addCompilationContract(registry, 42U, "integration", projectMaterializationContract);
}
}
