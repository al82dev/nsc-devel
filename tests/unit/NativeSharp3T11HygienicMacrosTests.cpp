#include "nativesharp/Frontend/MacroExpansionEngine.h"
#include "nativesharp/Frontend/ProjectCompilation.h"

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
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "nsc_ns3_t11_f04";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    writeFile(root / "Demo.nsproj", R"(<Project><PropertyGroup><Name>Demo</Name><Language>3</Language></PropertyGroup><ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>)");
    writeFile(root / "Main.ns", R"(
#macro identity(value) { $value }
#macro compute(value) { int $#temporary = #expand identity($value); return $#temporary + 2; }
namespace Meta;
public class Calculator
{
    public int First() { int temporary = 100; #expand compute(40) }
    public int Second() { int temporary = 200; #expand compute(40) }
}
)");

    ProjectCompilation compilation;
    std::string error;
    tests.expect(compilation.compileProjectMethodBodies(root / "Demo.nsproj", error),
        "hygienic syntax macros expand through the real parser and body analyzer");
    tests.expect(compilation.comptimeRecords().size() == 1 &&
            compilation.comptimeRecords()[0].macroExpansions.size() == 4,
        "nested expansions are recorded with deterministic trace depth");
    if (compilation.comptimeRecords().size() == 1)
    {
        const auto& expansions = compilation.comptimeRecords()[0].macroExpansions;
        tests.expect(expansions.size() == 4 && expansions[0].hygieneScope != expansions[2].hygieneScope,
            "independent macro invocations receive distinct hygiene scopes");
    }

    const auto direct = MacroExpansionEngine().expand(
        "#macro m(value) { int $#temp = $value; } void F(){ int temp=1; #expand m(2) }");
    tests.expect(direct.success && direct.source.find("int temp=1") != std::string::npos &&
            direct.source.find("__nsc_macro_1_temp") != std::string::npos,
        "introduced names cannot capture a same-spelled caller binding");
    const auto reserved = MacroExpansionEngine().expand("void F(){ int __nsc_macro_1_temp = 0; }");
    tests.expect(!reserved.success && reserved.error.find("reserved") != std::string::npos,
        "source cannot forge the compiler-reserved hygienic namespace");
    const auto recursive = MacroExpansionEngine(3).expand(
        "#macro again(value) { #expand again($value) } void F(){ #expand again(1) }");
    tests.expect(!recursive.success && recursive.error == "macro expansion limit exceeded",
        "recursive macro expansion stops at a deterministic bound");

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
