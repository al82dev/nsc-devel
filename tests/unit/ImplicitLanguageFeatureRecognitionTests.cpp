#include "nativesharp/Compilation/ProjectLoader.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
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
    std::string_view language)
{
    const std::filesystem::path directory = root / name;
    writeText(directory / "Main.ns", R"NS(
copy struct Marker
{
    public int Value;
}

public class Program
{
    public static int Main() { return 42; }
}
)NS");
    std::string project = "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><OutputType>Exe</OutputType>";
    if (!language.empty())
        project += "<Language>" + std::string(language) + "</Language>";
    project += "</PropertyGroup><ItemGroup><Compile Include=\"Main.ns\"/>"
        "</ItemGroup></Project>\n";
    const std::filesystem::path path = directory / (std::string(name) + ".nsproj");
    writeText(path, project);
    return path;
}

std::string diagnosticsFor(const nativesharp::frontend::ProjectCompilation& compilation)
{
    std::string text;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
        text += diagnostic.message() + " | ";
    return text;
}
}

int main()
{
    using nativesharp::language::LanguageEdition;
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_implicit_language_feature_recognition");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    const std::filesystem::path implicitProject =
        writeProject(root, "Implicit", "");
    std::string error;
    nativesharp::compilation::ProjectLoader loader;
    const auto implicitDescriptor = loader.load(implicitProject, error);
    tests.expect(implicitDescriptor.has_value() &&
            implicitDescriptor->languageEdition() == LanguageEdition::V2 &&
            implicitDescriptor->sourceLanguageEdition() == LanguageEdition::V2 &&
            implicitDescriptor->recognizedLanguageEdition() == LanguageEdition::V3 &&
            !implicitDescriptor->enforcesLanguage3Effects(),
        "an omitted edition keeps its graph default while recognizing the current source surface");

    nativesharp::frontend::ProjectCompilation implicitCompilation;
    error.clear();
    const bool implicitBuilt =
        implicitCompilation.compileProjectMethodBodies(implicitProject, error);
    tests.expect(implicitBuilt && error.empty() &&
            !implicitCompilation.diagnostics().hasErrors(),
        "an omitted edition recognizes explicit Copy without enabling mandatory Language 3 "
        "effects: " + error + diagnosticsFor(implicitCompilation));

    const std::filesystem::path explicitV2Project =
        writeProject(root, "ExplicitV2", "2");
    const auto explicitV2Descriptor = loader.load(explicitV2Project, error);
    tests.expect(explicitV2Descriptor.has_value() &&
            explicitV2Descriptor->sourceLanguageEdition() == LanguageEdition::V2 &&
            explicitV2Descriptor->recognizedLanguageEdition() == LanguageEdition::V2 &&
            !explicitV2Descriptor->enforcesLanguage3Effects(),
        "explicit Language 2 remains a frozen source contract");

    nativesharp::frontend::ProjectCompilation explicitV2Compilation;
    error.clear();
    const bool explicitV2Built =
        explicitV2Compilation.compileProjectMethodBodies(explicitV2Project, error);
    const std::string explicitV2Diagnostics =
        error + diagnosticsFor(explicitV2Compilation);
    tests.expect(!explicitV2Built &&
            explicitV2Diagnostics.find("requires <Language>3</Language>") != std::string::npos,
        "explicit Language 2 still rejects Language 3-only syntax: " +
            explicitV2Diagnostics);

    const std::filesystem::path explicitV3Project =
        writeProject(root, "ExplicitV3", "3");
    const auto explicitV3Descriptor = loader.load(explicitV3Project, error);
    tests.expect(explicitV3Descriptor.has_value() &&
            explicitV3Descriptor->sourceLanguageEdition() == LanguageEdition::V3 &&
            explicitV3Descriptor->recognizedLanguageEdition() == LanguageEdition::V3 &&
            explicitV3Descriptor->enforcesLanguage3Effects(),
        "explicit Language 3 selects both its source surface and effect policy");

    return tests.resultKeeping(root);
}
