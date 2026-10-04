#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Language/ModuleSemantics.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

nativesharp::ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    nativesharp::driver::Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const nativesharp::ExitCode result = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return result;
}
}

int main()
{
    using nativesharp::ExitCode;
    using namespace nativesharp::language;
    nativesharp::tests::TestContext tests;

    std::string error;
    std::vector<Import> normalized;
    const std::array<Import, 5> imports = {{{ImportKind::Extensions, "Demo.Extensions", ""},
        {ImportKind::Trait, "Demo.Contracts.Readable", "Readable"},
        {ImportKind::RestrictedGlob, "Demo.Public", ""},
        {ImportKind::Module, "Demo.Core", "Core"},
        {ImportKind::Symbol, "Demo.Core.Value", "V"}}};
    tests.expect(normalizeImports(imports, normalized, error) && normalized.size() == 5 &&
            normalized.front().alias.empty(),
        "explicit import kinds normalize independently of declaration order (" + error + ")");

    const std::array<Import, 2> collision = {{{ImportKind::Symbol, "A.Value", "Item"},
        {ImportKind::Trait, "B.Value", "Item"}}};
    tests.expect(!normalizeImports(collision, normalized, error) &&
            error.find("collision") != std::string::npos,
        "symbol and trait imports require explicit collision disambiguation");

    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t12_f02_imports";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) / "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());
    writeFile(root / "Imports.nsproj", R"XML(<Project><PropertyGroup>
  <Name>ExplicitImports</Name><OutputType>Exe</OutputType><Language>3</Language>
</PropertyGroup><ItemGroup><Compile Include="**/*.ns" /></ItemGroup></Project>)XML");
    writeFile(root / "Main.ns", R"NS(
namespace Imported
{
    public class Value { public static int Read() { return 40; } }
    public class Extra { public static int Read() { return 2; } }
}

// Directives are deliberately after the declarations. Binding must not depend on text order.
import symbol Imported.Value as V;
import module Imported;

public class Program
{
    public static int Main() { return V.Read() + Extra.Read(); }
}
)NS");

    const std::string project = (root / "Imports.nsproj").string();
    const std::string executable = (root / "Imports.exe").string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", project, "-O0", "-o", executable};
    std::string output;
    error.clear();
    const bool compiled = runDriver(arguments, output, error) == ExitCode::Success;
    tests.expect(compiled,
        "explicit module and symbol imports compile through the production frontend at O0 (" +
            error + output + ")");
    if (compiled)
        tests.expect(std::system(("\"" + executable + "\"").c_str()) == 42,
            "the imported alias and module names execute with stable lookup");

    writeFile(root / "Main.ns", R"NS(
namespace A { public class Value { } }
namespace B { public class Value { } }
import symbol A.Value as Item;
import symbol B.Value as Item;
public class Program { public static int Main() { return 0; } }
)NS");
    const std::string collisionProject = (root / "Imports.nsproj").string();
    const std::array<std::string_view, 3> collisionArguments = {
        "--check-project-oop-hir", collisionProject, "-O0"};
    output.clear();
    error.clear();
    const bool rejected = runDriver(collisionArguments, output, error) != ExitCode::Success;
    tests.expect(rejected && (error + output).find("duplicate using alias 'Item'") != std::string::npos,
        "a real source collision is rejected with its local alias (" + error + output + ")");

    return tests.resultKeeping(root);
}
