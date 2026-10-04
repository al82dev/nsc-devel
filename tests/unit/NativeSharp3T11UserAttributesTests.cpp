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

void writeProject(const std::filesystem::path& root, std::string_view source)
{
    writeFile(root / "Demo.nsproj", R"(<Project><PropertyGroup><Name>Demo</Name><Language>3</Language></PropertyGroup><ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>)");
    writeFile(root / "Main.ns", source);
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::frontend;
    const auto base = std::filesystem::path(NSC_BINARY_DIR) / "nsc_ns3_t11_f02";
    std::error_code ignored;
    std::filesystem::remove_all(base, ignored);

    const auto valid = base / "valid";
    writeProject(valid, R"(
#[attribute Demo.Tag targets(type|field) repeatable(false) retention(compiletime)]
#[attribute Demo.Native targets(method) repeatable(true) retention(runtime)]
namespace Meta;
#[Demo.Tag(7, "pair")]
public struct Pair
{
    #[Demo.Tag(true)]
    public int X;
    #[Demo.Native("entry")]
    public int Read() { return X; }
}
)");
    ProjectCompilation compilation;
    std::string error;
    tests.expect(compilation.compileProjectObjectLayouts(valid / "Demo.nsproj", error),
        "source-defined attributes pass through the productive frontend");
    const ProjectMetaprogrammingModel* model = compilation.metaprogrammingModel();
    tests.expect(model != nullptr && model->attributes().size() == 3,
        "attribute uses are attached to semantic type/member targets");
    if (model != nullptr && model->attributes().size() == 3)
    {
        tests.expect(model->attributes()[0].constantArguments.size() == 2 &&
                model->attributes()[0].constantArguments[0] == "7" &&
                model->attributes()[0].constantArguments[1] == "\"pair\"",
            "only constant attribute arguments survive into compile-time metadata");
        tests.expect(model->runtimeAttributes().size() == 1 &&
                model->runtimeAttributes()[0].identity == "Demo.Native",
            "runtime metadata is emitted only for explicit native-section retention");
    }

    const auto wrongTarget = base / "wrong-target";
    writeProject(wrongTarget, R"(
#[attribute Demo.FieldOnly targets(field) repeatable(false) retention(compiletime)]
namespace Meta;
#[Demo.FieldOnly]
public struct Pair { public int X; }
)");
    ProjectCompilation wrong;
    error.clear();
    tests.expect(!wrong.compileProjectObjectLayouts(wrongTarget / "Demo.nsproj", error) &&
            error.find("not valid") != std::string::npos,
        "attribute target validation rejects a field-only attribute on a type");

    const auto duplicate = base / "duplicate";
    writeProject(duplicate, R"(
#[attribute Demo.Once targets(type) repeatable(false) retention(compiletime)]
namespace Meta;
#[Demo.Once]
#[Demo.Once]
public struct Pair { public int X; }
)");
    ProjectCompilation repeated;
    error.clear();
    tests.expect(!repeated.compileProjectObjectLayouts(duplicate / "Demo.nsproj", error) &&
            error.find("non-repeatable") != std::string::npos,
        "non-repeatable attributes reject duplicate application deterministically");

    const auto nonConstant = base / "non-constant";
    writeProject(nonConstant, R"(
#[attribute Demo.Tag targets(type) repeatable(false) retention(compiletime)]
namespace Meta;
#[Demo.Tag(runtimeValue)]
public struct Pair { public int X; }
)");
    ProjectCompilation badArgument;
    error.clear();
    tests.expect(!badArgument.compileProjectSyntax(nonConstant / "Demo.nsproj", error) &&
            error.find("must be string, boolean, or integer constants") != std::string::npos,
        "non-constant attribute arguments fail before lexing");

    std::filesystem::remove_all(base, ignored);
    return tests.result();
}
