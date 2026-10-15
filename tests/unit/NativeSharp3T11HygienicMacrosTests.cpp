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
    tests.expect(!recursive.success && recursive.error.find("macro expansion limit exceeded") !=
            std::string::npos && recursive.error.find("comptime backtrace") != std::string::npos,
        "recursive macro expansion stops at a deterministic bound");

    const auto typed = MacroExpansionEngine().expand(
        "#macro make() expression { 40 + 2 } int v = #expand make();");
    tests.expect(typed.success && typed.expansions.size() == 1 &&
            typed.expansions[0].context == MacroExpansionContext::Expression &&
            typed.expansions[0].macroName == "make",
        "a macro can declare an expression context and the record carries it");
    const auto memberContext = MacroExpansionEngine().expand(
        "#macro field() member { public int X; } class C { #expand field(); }");
    tests.expect(memberContext.success && memberContext.expansions.size() == 1 &&
            memberContext.expansions[0].context == MacroExpansionContext::Member,
        "a member-context macro records the member kind");
    const auto bad = MacroExpansionEngine().expand(
        "#macro bad() nonsense { 1 } void F() { #expand bad(); }");
    tests.expect(!bad.success && bad.error.find("macro context must be statement, expression, member, or type") !=
            std::string::npos,
        "an unknown macro context is rejected before the AST is published");

    const auto mapped = MacroExpansionEngine().expand(
        "#macro inc(v) expression { $v + 1 } int a = #expand inc(1);");
    tests.expect(mapped.success && mapped.expansions.size() == 1 &&
            mapped.expansions[0].definitionOffset == 0 &&
            mapped.expansions[0].definitionLength != 0 &&
            mapped.expansions[0].invocationOffset > mapped.expansions[0].definitionOffset,
        "an expansion records both its invocation and its template source mapping");
    const std::string backtrace = macroExpansionBacktrace(mapped.expansions);
    tests.expect(backtrace.find("#expand inc") != std::string::npos &&
            backtrace.find("invocation offset") != std::string::npos &&
            backtrace.find("template offset") != std::string::npos,
        "the expansion backtrace navigates to both invocation and template without fictitious spans");

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
