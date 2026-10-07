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

std::filesystem::path writeConsumer(
    const std::filesystem::path& directory,
    std::string_view name,
    bool explicitLanguage3)
{
    std::string project = "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><OutputType>Exe</OutputType>";
    if (explicitLanguage3) project += "<Language>3</Language>";
    project += "</PropertyGroup><ItemGroup><Compile Include=\"Main.ns\"/>"
        "<ProjectReference Include=\"../Modern/Modern.nsproj\"/>"
        "</ItemGroup></Project>\n";
    writeText(directory / (std::string(name) + ".nsproj"), project);
    writeText(directory / "Main.ns", R"NS(
using Contracts;
public class Program
{
    public static int Main() { return Api.Dangerous(); }
}
)NS");
    return directory / (std::string(name) + ".nsproj");
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
        nativesharp::tests::evidenceRoot("nativesharp_implicit_language_semantics");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    writeText(root / "Modern/Modern.nsproj",
        "<Project><PropertyGroup><Name>Modern</Name><Language>3</Language>"
        "<OutputType>Library</OutputType></PropertyGroup><ItemGroup>"
        "<Compile Include=\"Api.ns\"/></ItemGroup></Project>\n");
    writeText(root / "Modern/Api.ns", R"NS(
namespace Contracts
{
    public class Api
    {
        public static unsafe int Dangerous() { return 42; }
    }
}
)NS");

    const std::filesystem::path implicitProject =
        writeConsumer(root / "Implicit", "Implicit", false);
    nativesharp::frontend::ProjectCompilation implicitCompilation;
    std::string error;
    const bool implicitBuilt =
        implicitCompilation.compileProjectMethodBodies(implicitProject, error);
    tests.expect(implicitBuilt && error.empty() &&
            !implicitCompilation.diagnostics().hasErrors(),
        "a consumer without Language accepts an unsafe-declared dependency without requiring "
        "unsafe at the call site: " + error + diagnosticsFor(implicitCompilation));
    tests.expect(implicitCompilation.projectGraph() != nullptr &&
            implicitCompilation.projectGraph()->projects().size() == 2U,
        "the source graph preserves both modules without an edition boundary");

    const std::filesystem::path explicitProject =
        writeConsumer(root / "Explicit", "Explicit", true);
    nativesharp::frontend::ProjectCompilation explicitCompilation;
    error.clear();
    const bool explicitBuilt =
        explicitCompilation.compileProjectMethodBodies(explicitProject, error);
    const std::string explicitDiagnostics = error + diagnosticsFor(explicitCompilation);
    tests.expect(explicitBuilt && explicitDiagnostics.empty(),
        "a historical Language property cannot force an unsafe writing style: " +
            explicitDiagnostics);

    return tests.resultKeeping(root);
}
