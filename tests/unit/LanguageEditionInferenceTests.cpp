#include "nativesharp/Compilation/ProjectGraphLoader.h"
#include "nativesharp/Compilation/ProjectLoader.h"
#include "nativesharp/Language/LanguageEdition.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace
{
void writeText(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

std::filesystem::path writeProject(
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view language,
    std::string_view references = {})
{
    const std::filesystem::path directory = root / name;
    writeText(directory / "Main.ns", "int Answer() { return 42; }\n");
    std::string project = "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><OutputType>Library</OutputType>";
    if (!language.empty())
        project += "<Language>" + std::string(language) + "</Language>";
    project += "</PropertyGroup><ItemGroup><Compile Include=\"Main.ns\"/>";
    project += references;
    project += "</ItemGroup></Project>\n";
    const std::filesystem::path path = directory / (std::string(name) + ".nsproj");
    writeText(path, project);
    return path;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    using nativesharp::language::LanguageEdition;

    const std::filesystem::path evidence =
        nativesharp::tests::evidenceRoot("nativesharp_language_edition_inference");
    std::error_code ignored;
    std::filesystem::remove_all(evidence, ignored);
    std::filesystem::create_directories(evidence);

    std::string error;
    nativesharp::compilation::ProjectLoader loader;
    const auto implicitProject = writeProject(evidence, "ImplicitStandalone", "");
    const auto loadedImplicit = loader.load(implicitProject, error);
    tests.expect(
        loadedImplicit.has_value() &&
            loadedImplicit->languageEdition() == LanguageEdition::V3 &&
            !loadedImplicit->hasExplicitLanguageEdition(),
        "an omitted Language selects the cumulative language surface: " + error);

    const auto explicitDependency = writeProject(evidence, "ExplicitDependency", "3");
    const std::string dependencyReference =
        "<ProjectReference Include=\"../ExplicitDependency/ExplicitDependency.nsproj\"/>";
    const auto implicitRoot =
        writeProject(evidence, "ImplicitRoot", "", dependencyReference);
    error.clear();
    const auto inferredGraph =
        nativesharp::compilation::ProjectGraphLoader().load(implicitRoot, error);
    bool allEdition3 = inferredGraph.has_value();
    bool ignoresAdministrativeEdition = inferredGraph.has_value();
    if (inferredGraph.has_value())
    {
        for (const auto& project : inferredGraph->projects())
        {
            allEdition3 = allEdition3 && project.languageEdition() == LanguageEdition::V3;
            if (project.moduleName() == "ImplicitRoot")
                ignoresAdministrativeEdition = ignoresAdministrativeEdition &&
                    !project.hasExplicitLanguageEdition();
            if (project.moduleName() == "ExplicitDependency")
                ignoresAdministrativeEdition = ignoresAdministrativeEdition &&
                    !project.hasExplicitLanguageEdition();
        }
    }
    tests.expect(allEdition3,
        "an implicit source project adopts the sole explicit graph edition: " + error);
    tests.expect(ignoresAdministrativeEdition,
        "source graph ignores administrative edition provenance");

    const auto explicitV2Root =
        writeProject(evidence, "ExplicitV2Root", "2", dependencyReference);
    error.clear();
    tests.expect(
        nativesharp::compilation::ProjectGraphLoader().load(explicitV2Root, error).has_value(),
        "mixed historical Language properties do not reject a source graph: " + error);

    return tests.resultKeeping(evidence);
}
