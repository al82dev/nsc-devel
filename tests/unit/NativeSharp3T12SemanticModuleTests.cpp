#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Compilation/ProjectGraphLoader.h"
#include "nativesharp/Driver/Driver.h"

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
    std::string_view file,
    std::string_view module,
    std::string_view output,
    std::string_view source,
    std::string_view references = {})
{
    writeFile(directory / file,
        "<Project><PropertyGroup><Name>" + std::string(module) + "</Name><OutputType>" +
        std::string(output) + "</OutputType></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" />" + std::string(references) +
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
    using nativesharp::compilation::ProjectGraphLoader;
    nativesharp::tests::TestContext tests;

    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t12_f01_semantic_modules";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) /
            "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    const std::filesystem::path cycleA = root / "binding-cycle/A";
    const std::filesystem::path cycleB = root / "binding-cycle/B";
    writeProject(cycleA, "A.nsproj", "CycleA", "Exe", R"NS(
using CycleB.Api;
namespace CycleA.Api
{
    public class FromA { public static int Value() { return FromB.Value() + Extra.Value(); } }
    public class Program { public static int Main() { return FromA.Value(); } }
}
)NS", "<ProjectReference Include=\"../B/B.nsproj\" />");
    writeFile(cycleA / "Extra.ns", R"NS(
namespace CycleA.Api
{
    public class Extra { public static int Value() { return 0; } }
}
)NS");
    writeProject(cycleB, "B.nsproj", "CycleB", "Library", R"NS(
using CycleA.Api;
namespace CycleB.Api
{
    public class FromB { public static int Value() { return 42; } }
    public class BindingWitness { public FromA Owner; }
    public class UnusedBomb
    {
        public static int Value = Trigger();
        public static int Trigger() { panic(); }
    }
}
)NS", "<ProjectReference Include=\"../A/A.nsproj\" />");

    ProjectGraphLoader loader;
    std::string error;
    const auto first = loader.load(cycleA / "A.nsproj", error);
    const std::string firstError = error;
    tests.expect(!first.has_value() &&
            firstError.find("project reference cycle: CycleA -> CycleB -> CycleA") !=
                std::string::npos,
        "project-reference cycles are rejected structurally (" + firstError + ")");
    const auto second = loader.load(cycleA / "A.nsproj", error);
    tests.expect(!second.has_value() && error == firstError,
        "project-cycle diagnostics are reproducible across repeated loads");

    const std::string cycleProject = (cycleA / "A.nsproj").string();
    const std::string cycleExe = (cycleA / "Cycle.exe").string();
    const std::array<std::string_view, 5> cycleArgs = {
        "--emit-project-oop-exe", cycleProject, "-O0", "-o", cycleExe};
    std::string output;
    error.clear();
    const bool cycleRejected = runDriver(cycleArgs, output, error) != ExitCode::Success;
    tests.expect(cycleRejected &&
            (error + output).find("project reference cycle: CycleA -> CycleB -> CycleA") !=
                std::string::npos,
        "the driver preserves the universal project-cycle diagnostic (" + error + output + ")");

    const std::filesystem::path layoutA = root / "layout-cycle/A";
    const std::filesystem::path layoutB = root / "layout-cycle/B";
    writeProject(layoutA, "A.nsproj", "LayoutA", "Exe", R"NS(
using LayoutB.Api;
namespace LayoutA.Api
{
    public struct AValue { public BValue Other; }
    public class Program { public static int Main() { return 0; } }
}
)NS", "<ProjectReference Include=\"../B/B.nsproj\" />");
    writeProject(layoutB, "B.nsproj", "LayoutB", "Library", R"NS(
using LayoutA.Api;
namespace LayoutB.Api { public struct BValue { public AValue Other; } }
)NS", "<ProjectReference Include=\"../A/A.nsproj\" />");
    const std::string layoutProject = (layoutA / "A.nsproj").string();
    const std::array<std::string_view, 3> layoutArgs = {
        "--check-project-oop-hir", layoutProject, "-O0"};
    output.clear();
    error.clear();
    const bool layoutRejected = runDriver(layoutArgs, output, error) != ExitCode::Success;
    tests.expect(layoutRejected,
        "a project cycle is rejected before recursive by-value layout analysis");
    tests.expect(layoutRejected &&
            (error + output).find("project reference cycle: LayoutA -> LayoutB -> LayoutA") !=
                std::string::npos,
        "the rejected layout graph reports its project-cycle cause (" + error + output + ")");

    const std::filesystem::path duplicate = root / "duplicate";
    writeProject(duplicate / "Upper", "Upper.nsproj", "SharedModule", "Library",
        "namespace Upper { public class Value { } }");
    writeProject(duplicate / "Lower", "Lower.nsproj", "sharedmodule", "Library",
        "namespace Lower { public class Value { } }");
    writeProject(duplicate / "App", "App.nsproj", "DuplicateApp", "Exe",
        "public class Program { public static int Main() { return 0; } }",
        "<ProjectReference Include=\"../Upper/Upper.nsproj\" />"
        "<ProjectReference Include=\"../Lower/Lower.nsproj\" />");
    error.clear();
    const auto duplicateGraph = loader.load(duplicate / "App/App.nsproj", error);
    tests.expect(!duplicateGraph.has_value() && error.find("duplicate module name") != std::string::npos,
        "Windows-case variants cannot claim the same canonical module identity");

    const std::filesystem::path casePath = root / "case-path";
    writeProject(casePath / "Dependency", "Dependency.nsproj", "CaseDependency", "Library",
        "namespace CaseDependency { public class Value { } }");
    writeProject(casePath / "App", "App.nsproj", "CasePathApp", "Exe",
        "public class Program { public static int Main() { return 0; } }",
        "<ProjectReference Include=\"../DEPENDENCY/DEPENDENCY.NSPROJ\" />");
    error.clear();
    const auto caseGraph = loader.load(casePath / "App/App.nsproj", error);
    tests.expect(caseGraph.has_value() && caseGraph->projects().size() == 2,
        "Windows project paths resolve case-insensitively without changing module identity (" + error + ")");

    const std::filesystem::path legacy = root / "legacy-cycle";
    writeFile(legacy / "A/A.nsproj",
        "<Project><PropertyGroup><Name>LegacyA</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" />"
        "<ProjectReference Include=\"../B/B.nsproj\" /></ItemGroup></Project>");
    writeFile(legacy / "A/Main.ns", "public class AValue { }\n");
    writeFile(legacy / "B/B.nsproj",
        "<Project><PropertyGroup><Name>LegacyB</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" />"
        "<ProjectReference Include=\"../A/A.nsproj\" /></ItemGroup></Project>");
    writeFile(legacy / "B/Main.ns", "public class BValue { }\n");
    error.clear();
    const auto legacyGraph = loader.load(legacy / "A/A.nsproj", error);
    tests.expect(!legacyGraph.has_value() && error.find("project reference cycle") != std::string::npos,
        "project cycles are rejected structurally, independently of historical edition input");

    return tests.resultKeeping(root);
}
