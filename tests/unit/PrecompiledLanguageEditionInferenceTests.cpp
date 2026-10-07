#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Compilation/ProjectGraphLoader.h"
#include "nativesharp/Language/LanguageEdition.h"

#include "../support/TestContext.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace
{
std::filesystem::path writeProject(
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view language)
{
    const std::filesystem::path directory = root / name;
    std::filesystem::create_directories(directory);
    std::ofstream(directory / "Main.ns", std::ios::binary | std::ios::trunc)
        << "int Answer() { return 42; }\n";
    std::ofstream project(directory / (std::string(name) + ".nsproj"),
        std::ios::binary | std::ios::trunc);
    project << "<Project><PropertyGroup><Name>" << name
        << "</Name><OutputType>Library</OutputType>";
    if (!language.empty()) project << "<Language>" << language << "</Language>";
    project << "</PropertyGroup><ItemGroup><Compile Include=\"Main.ns\"/>"
        << "</ItemGroup></Project>\n";
    return directory / (std::string(name) + ".nsproj");
}
}

int main()
{
    using nativesharp::compilation::PrecompiledProjectDescriptor;
    using nativesharp::compilation::ProjectGraphLoader;
    using nativesharp::language::LanguageEdition;
    nativesharp::tests::TestContext tests;
    const std::filesystem::path evidence =
        nativesharp::tests::evidenceRoot("nativesharp_precompiled_language_inference");
    std::error_code ignored;
    std::filesystem::remove_all(evidence, ignored);
    std::filesystem::create_directories(evidence);

    std::string error;
    const std::filesystem::path implicitProject = writeProject(evidence, "Implicit", "");
    auto implicitGraph = ProjectGraphLoader().load(implicitProject, error);
    const std::array edition3Metadata = {
        PrecompiledProjectDescriptor{
            implicitProject, evidence / "Core.nsmeta", "Core", LanguageEdition::V3}};
    const bool inferred = implicitGraph.has_value() &&
        implicitGraph->addPrecompiledProjects(edition3Metadata, error);
    tests.expect(inferred, "implicit source graph accepts one Language 3 NSMETA authority: " + error);
    tests.expect(inferred && implicitGraph->rootProject() != nullptr &&
            implicitGraph->rootProject()->languageEdition() == LanguageEdition::V3 &&
            !implicitGraph->rootProject()->hasExplicitLanguageEdition(),
        "NSMETA inference preserves omitted-language provenance");

    const std::filesystem::path explicitV2Project = writeProject(evidence, "ExplicitV2", "2");
    auto explicitV2Graph = ProjectGraphLoader().load(explicitV2Project, error);
    const std::array incompatibleMetadata = {
        PrecompiledProjectDescriptor{
            explicitV2Project, evidence / "Modern.nsmeta", "Modern", LanguageEdition::V3}};
    error.clear();
    tests.expect(explicitV2Graph.has_value() &&
            !explicitV2Graph->addPrecompiledProjects(incompatibleMetadata, error) &&
            error.find("NS3001") != std::string::npos,
        "explicit Language 2 remains incompatible with Language 3 NSMETA");

    auto conflictingGraph = ProjectGraphLoader().load(implicitProject, error);
    const std::array conflictingMetadata = {
        PrecompiledProjectDescriptor{
            implicitProject, evidence / "Legacy.nsmeta", "Legacy", LanguageEdition::V2},
        PrecompiledProjectDescriptor{
            implicitProject, evidence / "Modern.nsmeta", "Modern", LanguageEdition::V3}};
    error.clear();
    tests.expect(conflictingGraph.has_value() &&
            !conflictingGraph->addPrecompiledProjects(conflictingMetadata, error) &&
            error.find("NS3001") != std::string::npos,
        "an implicit graph rejects contradictory NSMETA language authorities");

    return tests.resultKeeping(evidence);
}
