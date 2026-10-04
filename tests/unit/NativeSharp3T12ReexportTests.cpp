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

void writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view output,
    std::string_view source,
    std::string_view references = {})
{
    writeFile(directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) + "</Name><OutputType>" +
        std::string(output) + "</OutputType><Language>3</Language></PropertyGroup><ItemGroup>"
        "<Compile Include=\"**/*.ns\" />" + std::string(references) +
        "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
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
    nativesharp::tests::TestContext tests;
    tests.expect(
        nativesharp::language::reexportedIdentity("Core::Api.Original", "Facade.Value") ==
            "Core::Api.Original",
        "a facade spelling preserves the original canonical identity");

    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t12_f04_reexports";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) / "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    const std::filesystem::path core = root / "Core";
    writeProject(core, "Core", "Library", R"NS(
namespace Core.Api
{
    public class Original { public static int Read() { return 42; } }
    public class Other { public static int Read() { return 7; } }
}
)NS");
    const std::string coreReference = "<ProjectReference Include=\"../Core/Core.nsproj\" />";

    const std::filesystem::path facadeA = root / "FacadeA";
    writeProject(facadeA, "FacadeA", "Library", R"NS(
export import symbol Core.Api.Original as Value;
public class MarkerA { }
)NS", coreReference);
    const std::filesystem::path facadeB = root / "FacadeB";
    writeProject(facadeB, "FacadeB", "Library", R"NS(
export import symbol Core.Api.Original as Value;
public class MarkerB { }
)NS", coreReference);
    const std::filesystem::path facadeC = root / "FacadeC";
    writeProject(facadeC, "FacadeC", "Library", R"NS(
export import symbol Core.Api.Other as Value;
public class MarkerC { }
)NS", coreReference);

    const std::string refA = "<ProjectReference Include=\"../FacadeA/FacadeA.nsproj\" />";
    const std::filesystem::path app = root / "App";
    writeProject(app, "App", "Exe", R"NS(
public class Program { public static int Main() { return Value.Read(); } }
)NS", refA);
    const std::string appProject = (app / "App.nsproj").string();
    const std::string appExe = (app / "App.exe").string();
    const std::array<std::string_view, 5> emit = {
        "--emit-project-oop-exe", appProject, "-O0", "-o", appExe};
    std::string output;
    std::string error;
    const bool compiled = runDriver(emit, output, error) == ExitCode::Success;
    tests.expect(compiled, "facade alias compiles at O0 through the production lookup (" +
            error + output + ")");
    if (compiled)
        tests.expect(std::system(("\"" + appExe + "\"").c_str()) == 42,
            "facade alias reaches the original implementation without a duplicate symbol");

    const std::filesystem::path diamond = root / "Diamond";
    writeProject(diamond, "Diamond", "Exe", R"NS(
public class Program { public static int Main() { return Value.Read(); } }
)NS", "<ProjectReference Include=\"../FacadeA/FacadeA.nsproj\" />"
       "<ProjectReference Include=\"../FacadeB/FacadeB.nsproj\" />");
    const std::string diamondProject = (diamond / "Diamond.nsproj").string();
    const std::array<std::string_view, 3> diamondCheck = {
        "--check-project-oop-hir", diamondProject, "-O0"};
    output.clear();
    error.clear();
    tests.expect(runDriver(diamondCheck, output, error) == ExitCode::Success,
        "diamond facades to the same identity deduplicate deterministically (" + error + output + ")");

    const std::filesystem::path conflict = root / "Conflict";
    writeProject(conflict, "Conflict", "Exe", R"NS(
public class Program { public static int Main() { Value item; return 0; } }
)NS", "<ProjectReference Include=\"../FacadeA/FacadeA.nsproj\" />"
       "<ProjectReference Include=\"../FacadeC/FacadeC.nsproj\" />");
    const std::string conflictProject = (conflict / "Conflict.nsproj").string();
    const std::array<std::string_view, 3> conflictCheck = {
        "--check-project-oop-hir", conflictProject, "-O0"};
    output.clear();
    error.clear();
    tests.expect(runDriver(conflictCheck, output, error) != ExitCode::Success &&
            (error + output).find("ambiguous") != std::string::npos,
        "facades that publish one name for different identities are rejected (" + error + output + ")");

    return tests.resultKeeping(root);
}
