#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMetaprogrammingModel.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string_view>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::frontend;
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "nsc_ns3_t11_f03";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    writeFile(root / "Demo.nsproj", R"(<Project><PropertyGroup><Name>Demo</Name><Language>3</Language></PropertyGroup><ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>)");
    writeFile(root / "Main.ns", R"(
#[attribute NativeSharp.DeriveIgnore targets(field) repeatable(false) retention(compiletime)]
namespace Meta;
public record struct Pair(int X, int Y) derive(Equality, Hash, Print, Clone)
{
    #[NativeSharp.DeriveIgnore]
    private int Cache;
}
)");
    ProjectCompilation compilation;
    std::string error;
    tests.expect(compilation.compileProjectObjectLayouts(root / "Demo.nsproj", error),
        "derive reaches parser, semantic member construction, attributes, and layout");
    const ProjectMetaprogrammingModel* model = compilation.metaprogrammingModel();
    tests.expect(model != nullptr && model->derives().size() == 4,
        "each explicit derive produces one typed generator record");
    if (model != nullptr && model->derives().size() == 4)
    {
        tests.expect(model->derives()[0].traitName == "Equality" &&
                model->derives()[1].traitName == "Hash" &&
                model->derives()[2].traitName == "Print" &&
                model->derives()[3].traitName == "Clone",
            "derive output uses canonical deterministic trait order");
        tests.expect(model->derives()[0].participatingFields.size() == 2 &&
                model->derives()[0].participatingFields[0].starts_with("X:") &&
                model->derives()[0].participatingFields[1].starts_with("Y:"),
            "DeriveIgnore removes a field from typed generator input");
        tests.expect(model->derives()[0].generatedMembers.size() == 3 &&
                model->derives()[3].generatedMembers == std::vector<std::string>{"Clone"},
            "derive output is restricted to the declared generated-member contract");
        tests.expect(model->derives()[0].dependencyKey.starts_with("derive:") &&
                model->derives()[0].dependencyKey != model->derives()[1].dependencyKey,
            "incremental dependency identity includes type, trait, and participating fields");
    }

    const auto bad = root / "bad";
    writeFile(bad / "Bad.nsproj", R"(<Project><PropertyGroup><Name>Bad</Name><Language>3</Language></PropertyGroup><ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>)");
    writeFile(bad / "Main.ns", "namespace Meta; public record struct Pair(int X) derive(Equality, Equality) {}\n");
    ProjectCompilation duplicate;
    error.clear();
    tests.expect(!duplicate.compileProjectSyntax(bad / "Bad.nsproj", error) &&
            duplicate.diagnostics().errorCount() != 0,
        "duplicate derive requests are diagnosed at their source declaration");

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
