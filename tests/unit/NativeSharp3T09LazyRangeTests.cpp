#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Language/IterationSemantics.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <span>
#include <string>
#include <string_view>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::frontend::ProjectCompilation;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

std::filesystem::path writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source,
    int edition = 3)
{
    const std::filesystem::path project = directory / (std::string(name) + ".nsproj");
    writeFile(project,
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>" + std::to_string(edition) +
        "</Language><OutputType>Exe</OutputType></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return project;
}

ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return result;
}

bool compileAndRun(const std::filesystem::path& project, int expected, std::string& diagnostic)
{
    const std::filesystem::path executable = project.parent_path() /
        (project.stem().string() + ".exe");
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", projectText, "-O0", "-o", executableText};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    if (result != ExitCode::Success)
    {
        diagnostic += "driver exit=" + std::to_string(static_cast<int>(result));
        return false;
    }
    const int observed = std::system(("\"" + executableText + "\"").c_str());
    if (observed == expected) return true;
    diagnostic += "process exit code " + std::to_string(observed) +
        ", expected " + std::to_string(expected);
    return false;
}

bool rejectedWith(
    const std::filesystem::path& project,
    std::string_view expected,
    std::string& diagnostic)
{
    const std::string projectText = project.string();
    const std::array<std::string_view, 3> arguments = {
        "--dump-project-method-bodies", projectText, "-O0"};
    std::string output;
    std::string error;
    const bool accepted = runDriver(arguments, output, error) == ExitCode::Success;
    diagnostic = error + output;
    return !accepted && diagnostic.find(expected) != std::string::npos;
}

bool dumpMir(
    const std::filesystem::path& project,
    std::string& output,
    std::string& diagnostic)
{
    const std::string projectText = project.string();
    const std::array<std::string_view, 3> arguments = {
        "--dump-project-oop-mir", projectText, "-O0"};
    std::string error;
    const bool accepted = runDriver(arguments, output, error) == ExitCode::Success;
    diagnostic = error;
    return accepted;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3-t09-f02-directed";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) /
            "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    const std::filesystem::path values = writeProject(root / "values", "LazyRanges", R"NS(
public class Program
{
    public static noheap int Main()
    {
        int total = 0;
        foreach (int value in 0..5) { total = total + value; }
        foreach (int value in 1..=5 step 2) { total = total + value; }
        foreach (int value in 10..0 step -3) { total = total + value; }
        const long first = 0;
        const long last = 1;
        foreach (long value in first..last) { total = total + (int)value; }
        return total + 1;
    }
}
)NS");
    std::string diagnostic;
    tests.expect(compileAndRun(values, 42, diagnostic),
        "O0/NSLink executes exclusive, inclusive, descending, const and wide ranges in noheap (" +
            diagnostic + ")");

    const std::filesystem::path order = writeProject(root / "order", "RangeOrder", R"NS(
public class Program
{
    private static int trace = 0;
    private static int Begin() { trace = trace * 10 + 1; return 0; }
    private static int End() { trace = trace * 10 + 2; return 3; }
    private static int Step() { trace = trace * 10 + 3; return 1; }
    public static int Main()
    {
        int sum = 0;
        foreach (int value in Begin()..End() step Step()) { sum = sum + value; }
        return trace == 123 && sum == 3 ? 42 : 1;
    }
}
)NS");
    diagnostic.clear();
    tests.expect(compileAndRun(order, 42, diagnostic),
        "range bounds and step are evaluated exactly once from left to right (" + diagnostic + ")");

    const std::filesystem::path zero = writeProject(root / "zero", "RangeZero", R"NS(
public class Program
{
    public static int Main()
    {
        foreach (int value in 0..10 step 0) { return value; }
        return 0;
    }
}
)NS");
    diagnostic.clear();
    tests.expect(rejectedWith(zero, "range step cannot be zero", diagnostic),
        "a constant zero step is diagnosed (" + diagnostic + ")");

    const std::filesystem::path direction = writeProject(root / "direction", "RangeDirection", R"NS(
public class Program
{
    public static int Main()
    {
        foreach (int value in 10..0 step 1) { return value; }
        return 0;
    }
}
)NS");
    diagnostic.clear();
    tests.expect(rejectedWith(direction, "range step moves away from its end", diagnostic),
        "a constant step with the wrong direction is diagnosed (" + diagnostic + ")");

    std::string rangeError;
    tests.expect(!nativesharp::language::lazyRangeCount(
            {std::numeric_limits<std::int64_t>::min(),
             std::numeric_limits<std::int64_t>::max(), 1, true}, rangeError).has_value() &&
            rangeError == "range element count overflows size_t",
        "an inclusive full-width range reports element-count overflow");

    std::string mir;
    diagnostic.clear();
    tests.expect(dumpMir(values, mir, diagnostic) &&
            !nativesharp::language::rangeMaterializesCollection() &&
            mir.find("new-object") == std::string::npos &&
            mir.find("array-new") == std::string::npos,
        "lazy range lowering uses scalar state and no collection allocation (" + diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
