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
void writeProject(const std::filesystem::path& root, std::string_view source)
{
    writeFile(root / "Demo.nsproj", R"(<Project><PropertyGroup><Name>Demo</Name><Language>3</Language></PropertyGroup><ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>)");
    writeFile(root / "Main.ns", source);
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp;
    const auto base = std::filesystem::path(NSC_BINARY_DIR) / "nsc_ns3_t11_f06";
    std::error_code ignored;
    std::filesystem::remove_all(base, ignored);

    const auto warnings = base / "warnings";
    writeProject(warnings, R"(
#compile_warning("deprecated surface", "DEMO-001")
#compile_note("use NewApi", "DEMO-002")
namespace Meta; public struct Value { public int X; }
)");
    frontend::ProjectCompilation warningCompilation;
    std::string error;
    tests.expect(warningCompilation.compileProjectSyntax(warnings / "Demo.nsproj", error),
        "warnings and notes do not stop productive compilation");
    tests.expect(warningCompilation.diagnostics().warningCount() == 1 &&
            warningCompilation.diagnostics().errorCount() == 0 &&
            warningCompilation.diagnostics().diagnostics().size() == 2,
        "user-authored warning and note preserve their semantic severities");
    tests.expect(warningCompilation.diagnostics().diagnostics()[0].message().find("[DEMO-001]") !=
            std::string::npos,
        "package code is preserved in the rendered diagnostic message");

    const auto macroError = base / "macro-error";
    writeProject(macroError, R"(
#macro reject() { #compile_error("generated rejection", "DEMO-003") }
#expand reject()
namespace Meta; public struct Value { public int X; }
)");
    frontend::ProjectCompilation failed;
    error.clear();
    tests.expect(!failed.compileProjectSyntax(macroError / "Demo.nsproj", error) &&
            failed.diagnostics().errorCount() == 1,
        "compile_error stops compilation through the ordinary diagnostic engine");
    if (!failed.diagnostics().diagnostics().empty())
    {
        const auto& diagnostic = failed.diagnostics().diagnostics().front();
        tests.expect(!diagnostic.notes().empty() &&
                diagnostic.notes()[0].find("expanded from macro 'reject'") != std::string::npos,
            "macro-generated diagnostics include deterministic expansion backtrace notes");
        tests.expect(diagnostic.range().has_value() && diagnostic.range()->isValid(),
            "user-authored diagnostics retain an authorized source range");
    }

    const auto collision = base / "collision";
    writeProject(collision, R"(
#compile_error("bad code", "NS3001")
namespace Meta; public struct Value { public int X; }
)");
    frontend::ProjectCompilation reserved;
    error.clear();
    tests.expect(!reserved.compileProjectSyntax(collision / "Demo.nsproj", error) &&
            error.find("collide with NS3xxx") != std::string::npos,
        "package diagnostics cannot collide with compiler-owned NS3xxx codes");

    const auto span = base / "span";
    writeProject(span, R"(
#compile_warning("bad span", "DEMO-004", 999999, 2)
namespace Meta; public struct Value { public int X; }
)");
    frontend::ProjectCompilation badSpan;
    error.clear();
    tests.expect(!badSpan.compileProjectSyntax(span / "Demo.nsproj", error) &&
            error.find("outside its source file") != std::string::npos,
        "authored spans are bounds-checked before entering diagnostics");

    std::filesystem::remove_all(base, ignored);
    return tests.result();
}
