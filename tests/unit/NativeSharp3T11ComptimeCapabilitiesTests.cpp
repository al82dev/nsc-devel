#include "nativesharp/Frontend/ComptimeEngine.h"
#include "nativesharp/Frontend/ProjectCompilation.h"

#include "../support/TestContext.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
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
    const auto base = std::filesystem::path(NSC_BINARY_DIR) / "nsc_ns3_t11_f05";
    std::error_code ignored;
    std::filesystem::remove_all(base, ignored);
    const auto first = base / "root-a";
    const auto second = base / "root-b";
    writeFile(first / "input.txt", "40\n");
    writeFile(second / "input.txt", "40\n");
    constexpr std::string_view source = R"(
#capability("file", "input.txt")
namespace Meta; public int Value() { return #run(file_i64("input.txt") + 2); }
)";

    const ComptimeResult a = ComptimeEngine({}, {}, {first, true}).execute(source);
    const ComptimeResult b = ComptimeEngine({}, {}, {second, true}).execute(source);
    tests.expect(a.success && b.success && a.reproducibilityDigest == b.reproducibilityDigest,
        "declared file input is root-independent and bit-identical across directories");
    tests.expect(a.evaluations.size() == 1 && a.evaluations[0].value == 42 &&
            a.dependencies.size() == 2 && a.dependencies[1].kind == "file" &&
            a.dependencies[1].identity == "input.txt",
        "file_i64 consumes only an explicitly declared project-relative input");

    const auto cache = std::make_shared<ComptimeCache>();
    const ComptimeResult cached = ComptimeEngine({}, cache, {first, true}).execute(source);
    writeFile(first / "input.txt", "41\n");
    const ComptimeResult changed = ComptimeEngine({}, cache, {first, true}).execute(source);
    tests.expect(cached.success && changed.success && changed.evaluations[0].value == 43 &&
            changed.cacheMisses == 1 && changed.reproducibilityDigest != cached.reproducibilityDigest,
        "external content digest invalidates the incremental #run cache");

    _putenv_s("NSC_T11_VALUE", "38");
    const ComptimeResult environment = ComptimeEngine({}, {}, {first, true}).execute(R"(
#capability("env", "NSC_T11_VALUE")
int Value() { return #run(env_i64("NSC_T11_VALUE") + 4); }
)");
    tests.expect(environment.success && environment.evaluations[0].value == 42 &&
            environment.dependencies[1].kind == "env",
        "declared environment input participates in evaluation and dependency identity");
    _putenv_s("NSC_T11_VALUE", "");

    const ComptimeResult undeclared = ComptimeEngine({}, {}, {first, true}).execute(
        "int Value() { return #run(file_i64(\"input.txt\")); }");
    tests.expect(!undeclared.success && undeclared.error.find("was not declared") != std::string::npos,
        "host input cannot be observed without an explicit capability");
    const ComptimeResult escape = ComptimeEngine({}, {}, {first, true}).execute(R"(
#capability("file", "../secret.txt")
int Value() { return #run(file_i64("../secret.txt")); }
)");
    tests.expect(!escape.success && escape.error.find("cannot escape") != std::string::npos,
        "declared file capabilities remain confined to the source root");
    const ComptimeResult clock = ComptimeEngine({}, {}, {first, true}).execute(
        "#capability(\"clock\", \"now\") int Value(){ return 0; }");
    tests.expect(!clock.success && clock.error.find("hermetic") != std::string::npos,
        "normative hermetic compilation rejects clock capability deterministically");

    writeFile(second / "Demo.nsproj", R"(<Project><PropertyGroup><Name>Demo</Name><Language>3</Language></PropertyGroup><ItemGroup><Compile Include="Main.ns" /></ItemGroup></Project>)");
    writeFile(second / "Main.ns", source);
    ProjectCompilation compilation;
    std::string error;
    tests.expect(compilation.compileProjectSyntax(second / "Demo.nsproj", error) &&
            compilation.comptimeRecords().size() == 1 &&
            compilation.comptimeRecords()[0].declaredCapabilities.size() == 1,
        "ProjectCompilation carries declared capabilities and their dependency evidence");

    std::filesystem::remove_all(base, ignored);
    return tests.result();
}
