#include "nativesharp/Compilation/GlobPattern.h"
#include "nativesharp/Compilation/ProjectDumper.h"
#include "nativesharp/Compilation/ProjectLoader.h"
#include "nativesharp/Compilation/SimpleXmlParser.h"

#include "../support/TestContext.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
using nativesharp::compilation::GlobPattern;
using nativesharp::compilation::ProjectDumper;
using nativesharp::compilation::ProjectLoadOptions;
using nativesharp::compilation::ProjectLoader;
using nativesharp::compilation::ProjectOutputKind;
using nativesharp::compilation::SimpleXmlElement;
using nativesharp::compilation::SimpleXmlParser;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void testGlobPattern(TestContext& tests)
{
    const GlobPattern recursive("**/*.ns");
    tests.expect(recursive.isValid(), "recursive NativeSharp glob is valid");
    tests.expect(recursive.matches("Main.ns"), "recursive glob matches root source");
    tests.expect(recursive.matches("Models/Player.ns"), "recursive glob matches nested source");
    tests.expect(recursive.matches("models/player.NS"), "glob path matching is case-insensitive");

    const GlobPattern oneDirectory("src/*.ns");
    tests.expect(oneDirectory.matches("src/Main.ns"), "single star matches within one path segment");
    tests.expect(!oneDirectory.matches("src/Models/Player.ns"), "single star does not cross path segments");

    const GlobPattern question("Models/Player?.ns");
    tests.expect(question.matches("Models/Player1.ns"), "question mark matches one character");
    tests.expect(!question.matches("Models/Player10.ns"), "question mark does not match two characters");

    tests.expect(!GlobPattern("../Shared/*.ns").isValid(), "parent traversal glob is invalid");
    tests.expect(!GlobPattern("C:/Project/*.ns").isValid(), "absolute drive glob is invalid");
    tests.expect(!GlobPattern("Models/**Player.ns").isValid(), "double star must occupy a segment");
}

void testSimpleXml(TestContext& tests)
{
    SimpleXmlParser parser;
    SimpleXmlElement root;
    std::string error;
    const std::string valid =
        "<?xml version=\"1.0\"?>\n"
        "<!-- project -->\n"
        "<Project Label=\"A&amp;B\"><Property>Value</Property></Project>";
    tests.expect(parser.parse(valid, root, error), "minimal XML parser accepts declaration and comment");
    tests.expect(root.name == "Project", "XML parser returns root name");
    tests.expect(
        root.attributes.size() == 1 && root.attributes[0].second == "A&B",
        "XML parser decodes predefined entities");
    tests.expect(
        root.children.size() == 1 && root.children[0].text == "Value",
        "XML parser preserves property text");

    tests.expect(!parser.parse("<Project></Wrong>", root, error), "mismatched XML close is rejected");
    tests.expect(
        error.find("does not match") != std::string::npos,
        "mismatched XML close has deterministic diagnostic");
    tests.expect(!parser.parse("<Project A=\"1\" A=\"2\"/>", root, error), "duplicate XML attribute is rejected");
    tests.expect(!parser.parse("<Project>&unknown;</Project>", root, error), "unknown XML entity is rejected");

    std::string hostile;
    for (std::size_t depth = 0; depth < 129U; ++depth) hostile += "<N>";
    for (std::size_t depth = 0; depth < 129U; ++depth) hostile += "</N>";
    tests.expect(!parser.parse(hostile, root, error), "hostile XML nesting is rejected before stack exhaustion");
    tests.expect(error.find("128-element safety limit") != std::string::npos,
        "hostile XML nesting reports its explicit resource limit");
}

void testDefaultProject(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path projectDirectory = root / "Game";
    writeFile(projectDirectory / "Main.ns", "int main() { return 0; }\n");
    writeFile(projectDirectory / "Models" / "Player.ns", "// future partial class\n");
    writeFile(projectDirectory / "bin" / "Generated.ns", "// ignored\n");
    writeFile(projectDirectory / "obj" / "Temporary.ns", "// ignored\n");
    writeFile(projectDirectory / "notes.txt", "ignored\n");
    writeFile(
        projectDirectory / "Game.nsproj",
        "<?xml version=\"1.0\"?>\n"
        "<Project>\n"
        "  <PropertyGroup>\n"
        "    <Name>Game</Name>\n"
        "    <OutputType>Library</OutputType>\n"
        "  </PropertyGroup>\n"
        "</Project>\n");

    ProjectLoader loader;
    std::string error;
    const auto project = loader.load(projectDirectory / "Game.nsproj", error);
    tests.expect(project.has_value(), "default project loads");
    if (!project.has_value())
    {
        return;
    }
    tests.expect(project->moduleName() == "Game", "project Name becomes module identity");
    tests.expect(project->outputKind() == ProjectOutputKind::Library, "Library output kind is parsed");
    tests.expect(project->sourceSet().isFinalized(), "loaded project source set is frozen");
    tests.expect(project->sourceSet().sources().size() == 2, "default glob discovers two user sources");
    tests.expect(
        project->sourceSet().sources()[0].projectRelativePath.generic_string() == "Main.ns",
        "default discovery is canonically sorted");
    tests.expect(
        project->sourceSet().sources()[1].projectRelativePath.generic_string() == "Models/Player.ns",
        "default discovery includes nested source");

    std::ostringstream dump;
    ProjectDumper().dump(*project, dump);
    tests.expect(dump.str().find("project Game\n") == 0, "project dump begins with module name");
    tests.expect(dump.str().find("output: static-library") != std::string::npos, "project dump contains output kind");
    tests.expect(dump.str().find("[1] Models/Player.ns") != std::string::npos, "project dump lists ordinals");
}

void testItemsAndReferences(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path core = root / "Core";
    writeFile(core / "Core.ns", "int GetValue() { return 42; }\n");
    writeFile(core / "Core.nsproj", "<Project><PropertyGroup><Name>Core</Name></PropertyGroup></Project>\n");

    const std::filesystem::path app = root / "Filtered";
    const std::filesystem::path precompiled = root / "Precompiled";
    writeFile(precompiled / "NativeMath.lib", "archive");
    writeFile(precompiled / "NativeMath.nsmeta", "metadata");
    writeFile(precompiled / "NativeMath.dll", "runtime");
    writeFile(app / "Main.ns", "int main() { return 0; }\n");
    writeFile(app / "Models" / "Player.ns", "// player\n");
    writeFile(app / "Models" / "Ignored.ns", "// ignored\n");
    writeFile(app / "Other.ns", "// excluded by include\n");
    writeFile(
        app / "Filtered.nsproj",
        "<Project>\n"
        "  <PropertyGroup><Name>Filtered</Name><OutputType>Exe</OutputType></PropertyGroup>\n"
        "  <ItemGroup>\n"
        "    <Compile Include=\"Models/**/*.ns;Main.ns\" Exclude=\"**/Ignored.ns\" />\n"
        "    <Compile Include=\"Main.ns\" />\n"
        "    <Compile Remove=\"Main.ns\" />\n"
        "    <ProjectReference Include=\"../Core/Core.nsproj\" />\n"
        "    <LibraryReference Include=\"../Precompiled/NativeMath.lib\" "
            "Metadata=\"../Precompiled/NativeMath.nsmeta\" Runtime=\"../Precompiled/NativeMath.dll\" />\n"
        "  </ItemGroup>\n"
        "</Project>\n");

    ProjectLoader loader;
    std::string error;
    const auto project = loader.load(app / "Filtered.nsproj", error);
    tests.expect(project.has_value(), "filtered project with reference loads");
    if (!project.has_value())
    {
        return;
    }
    tests.expect(project->outputKind() == ProjectOutputKind::Executable, "Exe output kind is parsed");
    tests.expect(project->sourceSet().sources().size() == 1, "include, exclude, and remove compose");
    tests.expect(
        project->sourceSet().sources()[0].projectRelativePath.generic_string() == "Models/Player.ns",
        "filtered source is the expected file");
    tests.expect(project->projectReferences().size() == 1, "project reference is preserved");
    tests.expect(
        project->projectReferences()[0].filename() == "Core.nsproj",
        "project reference is canonicalized");
    tests.expect(project->libraryReferences().size() == 1,
        "precompiled library reference is preserved separately from source project references");
    tests.expect(project->libraryReferences()[0].libraryPath.filename() == "NativeMath.lib" &&
            project->libraryReferences()[0].metadataPath.filename() == "NativeMath.nsmeta" &&
            project->libraryReferences()[0].runtimePath.filename() == "NativeMath.dll",
        "library, metadata, and optional runtime paths are canonicalized as one contract");
}

void expectLoadFailure(
    TestContext& tests,
    const std::filesystem::path& project,
    std::string_view expectedText,
    std::string_view message)
{
    ProjectLoader loader;
    std::string error;
    const auto result = loader.load(project, error);
    tests.expect(!result.has_value(), message);
    tests.expect(
        error.find(expectedText) != std::string::npos,
        std::string(message) + " has deterministic diagnostic");
}

void testInvalidProjects(TestContext& tests, const std::filesystem::path& root)
{
    expectLoadFailure(tests, root / "Missing.nsproj", "does not exist", "missing project is rejected");

    const std::filesystem::path invalid = root / "Invalid";
    writeFile(invalid / "Main.ns", "int main() { return 0; }\n");

    writeFile(invalid / "Wrong.txt", "<Project/>\n");
    expectLoadFailure(tests, invalid / "Wrong.txt", ".nsproj extension", "wrong project extension is rejected");

    writeFile(invalid / "Malformed.nsproj", "<Project>\n");
    expectLoadFailure(tests, invalid / "Malformed.nsproj", "not closed", "malformed XML is rejected");

    writeFile(invalid / "Root.nsproj", "<Solution/>\n");
    expectLoadFailure(tests, invalid / "Root.nsproj", "root element", "wrong XML root is rejected");

    writeFile(invalid / "Property.nsproj", "<Project><PropertyGroup><Unknown>x</Unknown></PropertyGroup></Project>\n");
    expectLoadFailure(
        tests,
        invalid / "Property.nsproj",
        "unsupported project property",
        "unknown property is rejected");

    writeFile(invalid / "Name.nsproj", "<Project><PropertyGroup><Name>bad-name</Name></PropertyGroup></Project>\n");
    expectLoadFailure(
        tests,
        invalid / "Name.nsproj",
        "valid NativeSharp identifier",
        "invalid module name is rejected");

    writeFile(
        invalid / "Output.nsproj",
        "<Project><PropertyGroup><OutputType>Plugin</OutputType></PropertyGroup></Project>\n");
    expectLoadFailure(
        tests,
        invalid / "Output.nsproj",
        "Exe', 'Library', 'StaticLibrary', or 'SharedLibrary",
        "invalid output kind is rejected");

    writeFile(invalid / "Glob.nsproj", "<Project><ItemGroup><Compile Include=\"../*.ns\"/></ItemGroup></Project>\n");
    expectLoadFailure(tests, invalid / "Glob.nsproj", "invalid glob pattern", "escaping glob is rejected");

    writeFile(invalid / "Compile.nsproj", "<Project><ItemGroup><Compile/></ItemGroup></Project>\n");
    expectLoadFailure(tests, invalid / "Compile.nsproj", "exactly one", "empty Compile item is rejected");

    writeFile(
        invalid / "Empty.nsproj",
        "<Project><ItemGroup><Compile Remove=\"**/*.ns\"/></ItemGroup></Project>\n");
    expectLoadFailure(tests, invalid / "Empty.nsproj", "at least one", "project with no selected source is rejected");

    writeFile(
        invalid / "Reference.nsproj",
        "<Project><ItemGroup><ProjectReference Include=\"Missing.nsproj\"/></ItemGroup></Project>\n");
    expectLoadFailure(
        tests,
        invalid / "Reference.nsproj",
        "referenced project does not exist",
        "missing reference is rejected");

    const std::filesystem::path reference = root / "ReferenceTarget";
    writeFile(reference / "Target.ns", "// target\n");
    writeFile(reference / "Target.nsproj", "<Project/>\n");
    writeFile(
        invalid / "DuplicateReference.nsproj",
        "<Project><ItemGroup>\n"
        "<ProjectReference Include=\"../ReferenceTarget/Target.nsproj\"/>\n"
        "<ProjectReference Include=\"../ReferenceTarget/Target.nsproj\"/>\n"
        "</ItemGroup></Project>\n");
    expectLoadFailure(
        tests,
        invalid / "DuplicateReference.nsproj",
        "duplicate ProjectReference",
        "duplicate reference is rejected");

    writeFile(
        invalid / "MissingLibrary.nsproj",
        "<Project><ItemGroup><LibraryReference Include=\"Missing.lib\"/></ItemGroup></Project>\n");
    expectLoadFailure(
        tests,
        invalid / "MissingLibrary.nsproj",
        "referenced library does not exist",
        "missing precompiled library is rejected");

    const std::filesystem::path libraryReference = root / "LibraryReferenceTarget";
    writeFile(libraryReference / "Target.lib", "archive");
    writeFile(
        invalid / "MissingMetadata.nsproj",
        "<Project><ItemGroup><LibraryReference Include=\"../LibraryReferenceTarget/Target.lib\"/>"
        "</ItemGroup></Project>\n");
    expectLoadFailure(
        tests,
        invalid / "MissingMetadata.nsproj",
        "metadata does not exist",
        "a bare .lib without NativeSharp metadata is rejected");

    writeFile(libraryReference / "Target.nsmeta", "metadata");
    writeFile(
        invalid / "DuplicateLibrary.nsproj",
        "<Project><ItemGroup>"
        "<LibraryReference Include=\"../LibraryReferenceTarget/Target.lib\"/>"
        "<LibraryReference Include=\"../LibraryReferenceTarget/Target.lib\"/>"
        "</ItemGroup></Project>\n");
    expectLoadFailure(
        tests,
        invalid / "DuplicateLibrary.nsproj",
        "duplicate LibraryReference",
        "duplicate precompiled library reference is rejected");
}

void testLibraryReferenceRoot(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path app = root / "ReferenceRootApp";
    const std::filesystem::path o0 = root / "build" / "O0";
    const std::filesystem::path o2 = root / "build" / "O2";
    writeFile(app / "Main.ns", "int Main() { return 0; }\n");
    writeFile(
        app / "App.nsproj",
        "<Project><PropertyGroup><Name>App</Name></PropertyGroup><ItemGroup>"
        "<LibraryReference Include=\"dependencies/NativeMath.lib\" "
        "Metadata=\"dependencies/NativeMath.nsmeta\" "
        "Runtime=\"dependencies/NativeMath.dll\"/>"
        "</ItemGroup></Project>\n");
    for (const std::filesystem::path& configuration : {o0, o2})
    {
        writeFile(configuration / "dependencies" / "NativeMath.lib", "archive");
        writeFile(configuration / "dependencies" / "NativeMath.nsmeta", "metadata");
        writeFile(configuration / "dependencies" / "NativeMath.dll", "runtime");
    }

    ProjectLoader loader;
    std::string error;
    ProjectLoadOptions options;
    options.libraryReferenceRoot = o2;
    const auto project = loader.load(app / "App.nsproj", options, error);
    tests.expect(project.has_value(), "library-reference root loads a configuration-specific dependency set");
    if (!project.has_value()) return;
    tests.expect(project->libraryReferences().size() == 1,
        "library-reference root preserves one precompiled dependency");
    const auto& reference = project->libraryReferences()[0];
    tests.expect(reference.libraryPath == std::filesystem::weakly_canonical(
            o2 / "dependencies" / "NativeMath.lib") &&
            reference.metadataPath == std::filesystem::weakly_canonical(
                o2 / "dependencies" / "NativeMath.nsmeta") &&
            reference.runtimePath == std::filesystem::weakly_canonical(
                o2 / "dependencies" / "NativeMath.dll"),
        "Include, Metadata, and Runtime resolve from the selected root without rewriting the project");
    tests.expect(reference.libraryPath.string().find(o0.string()) == std::string::npos,
        "the selected O2 root cannot silently retain the O0 dependency path");

    ProjectLoadOptions fallbackOptions;
    fallbackOptions.libraryReferenceFallbackRoot = o2;
    const auto fallbackProject = loader.load(app / "App.nsproj", fallbackOptions, error);
    tests.expect(fallbackProject.has_value(),
        "a fallback root supplies a dependency absent beside the project");
    if (fallbackProject.has_value())
        tests.expect(fallbackProject->libraryReferences()[0].libraryPath ==
                std::filesystem::weakly_canonical(o2 / "dependencies" / "NativeMath.lib"),
            "the fallback root supplies the missing relative dependency");

    char* previousEnvironmentRoot = nullptr;
    std::size_t previousEnvironmentRootLength = 0;
    _dupenv_s(
        &previousEnvironmentRoot, &previousEnvironmentRootLength, "NS_LIB_ROOT");
    const std::string savedEnvironmentRoot =
        previousEnvironmentRoot != nullptr ? previousEnvironmentRoot : "";
    std::free(previousEnvironmentRoot);
    _putenv_s("NS_LIB_ROOT", o2.string().c_str());
    const auto environmentProject = loader.load(app / "App.nsproj", error);
    tests.expect(environmentProject.has_value(),
        "NS_LIB_ROOT supplies the fallback to direct ProjectLoader clients");
    if (environmentProject.has_value())
        tests.expect(environmentProject->libraryReferences()[0].libraryPath ==
                std::filesystem::weakly_canonical(o2 / "dependencies" / "NativeMath.lib"),
            "the environment fallback resolves the same package artifacts as explicit options");
    _putenv_s("NS_LIB_ROOT", savedEnvironmentRoot.c_str());

    writeFile(app / "dependencies" / "NativeMath.lib", "local archive");
    writeFile(app / "dependencies" / "NativeMath.nsmeta", "local metadata");
    writeFile(app / "dependencies" / "NativeMath.dll", "local runtime");
    const auto localProject = loader.load(app / "App.nsproj", fallbackOptions, error);
    tests.expect(localProject.has_value(),
        "a project-relative dependency remains valid when a fallback root is configured");
    if (localProject.has_value())
        tests.expect(localProject->libraryReferences()[0].libraryPath ==
                std::filesystem::weakly_canonical(app / "dependencies" / "NativeMath.lib"),
            "fallback roots never rebase an existing project-relative dependency");
}

void testBuildSettings(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "BuildSettings";
    writeFile(directory / "Main.ns", "int Main() { return 0; }\n");
    writeFile(
        directory / "Configured.nsproj",
        "<Project><PropertyGroup>"
        "<Name>Configured</Name><OutputType>Library</OutputType>"
        "<OptimizationLevel>O0</OptimizationLevel>"
        "<OutputPath>../products/Configured.lib</OutputPath>"
        "<LibraryReferenceRoot>../dependencies/O0</LibraryReferenceRoot>"
        "<CompilerJobs>12</CompilerJobs>"
        "<GenericInstantiationDepth>96</GenericInstantiationDepth>"
        "<GenericInstantiationLimit>32768</GenericInstantiationLimit>"
        "</PropertyGroup></Project>\n");

    ProjectLoader loader;
    std::string error;
    const auto settings = loader.loadBuildSettings(directory / "Configured.nsproj", error);
    tests.expect(settings.has_value(), "project build settings load without source discovery");
    if (settings.has_value())
    {
        tests.expect(settings->optimizationLevel == 0U,
            "OptimizationLevel is parsed as the requested LLVM level");
        tests.expect(settings->compilerJobs == 12U,
            "CompilerJobs is parsed as the requested concurrency");
        tests.expect(settings->genericInstantiationDepth == 96U &&
                settings->genericInstantiationCount == 32768U,
            "generic instantiation budgets are parsed as project defaults");
        tests.expect(settings->outputPath == (root / "products" / "Configured.lib").lexically_normal(),
            "relative OutputPath resolves from the project directory");
        tests.expect(settings->libraryReferenceRoot ==
                (root / "dependencies" / "O0").lexically_normal(),
            "relative LibraryReferenceRoot resolves from the project directory");
    }
    tests.expect(loader.load(directory / "Configured.nsproj", error).has_value(),
        "the normal project loader accepts build properties");

    writeFile(
        directory / "BadOptimization.nsproj",
        "<Project><PropertyGroup><OptimizationLevel>fast</OptimizationLevel>"
        "</PropertyGroup></Project>\n");
    tests.expect(!loader.loadBuildSettings(directory / "BadOptimization.nsproj", error).has_value() &&
            error.find("OptimizationLevel must be") != std::string::npos,
        "invalid project optimization has a deterministic diagnostic");

    writeFile(
        directory / "BadJobs.nsproj",
        "<Project><PropertyGroup><CompilerJobs>0</CompilerJobs>"
        "</PropertyGroup></Project>\n");
    tests.expect(!loader.loadBuildSettings(directory / "BadJobs.nsproj", error).has_value() &&
            error.find("1 through 1024") != std::string::npos,
        "invalid project job count has a deterministic diagnostic");

    writeFile(
        directory / "BadGenericBudget.nsproj",
        "<Project><PropertyGroup><GenericInstantiationDepth>0</GenericInstantiationDepth>"
        "</PropertyGroup></Project>\n");
    tests.expect(!loader.loadBuildSettings(
            directory / "BadGenericBudget.nsproj", error).has_value() &&
            error.find("GenericInstantiationDepth must be") != std::string::npos,
        "invalid project generic budgets have deterministic diagnostics");
}
}

int main()
{
    TestContext tests;
    testGlobPattern(tests);
    testSimpleXml(tests);

    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage22_project_loader";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);

    testDefaultProject(tests, root);
    testItemsAndReferences(tests, root);
    testLibraryReferenceRoot(tests, root);
    testBuildSettings(tests, root);
    testInvalidProjects(tests, root);

    std::filesystem::remove_all(root, error);
    return tests.result();
}
