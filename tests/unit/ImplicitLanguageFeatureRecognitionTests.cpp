#include "nativesharp/Compilation/ProjectLoader.h"
#include "nativesharp/Frontend/ProjectCompilation.h"

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
            implicitDescriptor->moduleName() == "Implicit" &&
            implicitDescriptor->sourceSet().sources().size() == 1U,
        "an omitted Language recognizes the cumulative source surface");

    nativesharp::frontend::ProjectCompilation implicitCompilation;
    error.clear();
    const bool implicitBuilt =
        implicitCompilation.compileProjectMethodBodies(implicitProject, error);
    tests.expect(implicitBuilt && error.empty() &&
            !implicitCompilation.diagnostics().hasErrors(),
        "an omitted Language recognizes explicit Copy without mandatory effects: " +
            error + diagnosticsFor(implicitCompilation));

    const std::filesystem::path explicitLegacyProject =
        writeProject(root, "ExplicitLegacy", "legacy-a");
    const auto explicitLegacyDescriptor = loader.load(explicitLegacyProject, error);
    tests.expect(explicitLegacyDescriptor.has_value() &&
            explicitLegacyDescriptor->moduleName() == "ExplicitLegacy" &&
            explicitLegacyDescriptor->sourceSet().sources().size() == 1U,
        "a historical Language property does not freeze the source contract");

    nativesharp::frontend::ProjectCompilation explicitLegacyCompilation;
    error.clear();
    const bool explicitLegacyBuilt =
        explicitLegacyCompilation.compileProjectMethodBodies(explicitLegacyProject, error);
    const std::string explicitLegacyDiagnostics =
        error + diagnosticsFor(explicitLegacyCompilation);
    tests.expect(explicitLegacyBuilt && explicitLegacyDiagnostics.empty(),
        "a historical Language property accepts current syntax: " + explicitLegacyDiagnostics);

    const std::filesystem::path explicitOtherProject =
        writeProject(root, "ExplicitOther", "legacy-b");
    const auto explicitOtherDescriptor = loader.load(explicitOtherProject, error);
    tests.expect(explicitOtherDescriptor.has_value() &&
            explicitOtherDescriptor->moduleName() == "ExplicitOther" &&
            explicitOtherDescriptor->sourceSet().sources().size() == 1U,
        "another Language property value is also administrative and does not force effects");

    return tests.resultKeeping(root);
}
