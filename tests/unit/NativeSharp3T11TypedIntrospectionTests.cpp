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
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "nsc_ns3_t11_f01";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    writeFile(root / "Demo.nsproj", R"(<Project><PropertyGroup><Name>Demo</Name><Language>3</Language></PropertyGroup><ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>)");
    writeFile(root / "Types.ns", R"(
namespace Meta;
public struct Pair
{
    private int Hidden;
    public int X;
    public long Y;
    public int Sum() { return X + (int)Y; }
}
internal struct Secret { public int Value; }
)");

    ProjectCompilation compilation;
    std::string error;
    tests.expect(compilation.compileProjectObjectLayouts(root / "Demo.nsproj", error),
        "F01 project reaches the real names, members, and layout pipeline");
    const ProjectMetaprogrammingModel* model = compilation.metaprogrammingModel();
    tests.expect(model != nullptr && model->isBuilt(),
        "typed introspection is attached to ProjectCompilation");
    if (model != nullptr)
    {
        const StaticTypeInfo* pair = model->findType("Demo::Meta.Pair", 99);
        tests.expect(pair != nullptr && pair->size == 16 && pair->alignment == 8,
            "authorized compile-time layout is exposed as typed scalar data");
        if (pair != nullptr)
        {
            const auto external = model->visibleMembers(*pair, 99);
            tests.expect(external.size() == 3 && external[0].name == "X" &&
                    external[1].name == "Y" && external[2].name == "Sum",
                "external introspection is source-stable and filters private members");
            const auto internal = model->visibleMembers(*pair, pair->ownerProjectOrdinal);
            tests.expect(internal.size() == 4 && internal[0].name == "Hidden",
                "same-module introspection can inspect private members");
        }
        tests.expect(model->findType("Demo::Meta.Secret", 99) == nullptr,
            "private module types remain invisible to external introspection");
        tests.expect(!model->emitsRuntimeMetadata(),
            "typed introspection does not request a runtime reflection registry");
    }
    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
