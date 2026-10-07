#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <span>
#include <string>
#include <string_view>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;

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

bool dumpMir(const std::filesystem::path& project, std::string& output, std::string& diagnostic)
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
        std::filesystem::path(NSC_BINARY_DIR) / "ns3-t09-f03-directed";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) /
            "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    constexpr std::string_view prelude = R"NS(
public copy struct SumBuilder
{
    public int Total;
    public noheap void Add(int value) { Total = Total + value; }
}
)NS";

    const std::filesystem::path filtered = writeProject(
        root / "filtered", "ComprehensionFiltered", std::string(prelude) + R"NS(
public class Program
{
    public static noheap int Main()
    {
        SumBuilder seed;
        seed.Total = 0;
        SumBuilder result = comprehend(seed) {
            foreach (int value in 0..10) {
                if ((value % 2) == 0) yield return value;
            }
        };
        return result.Total == 20 ? 42 : 1;
    }
}
)NS");
    std::string diagnostic;
    const bool filteredAccepted = compileAndRun(filtered, 42, diagnostic);
    tests.expect(filteredAccepted,
        "O0/NSLink executes a filtered lazy-range comprehension in noheap (" + diagnostic + ")");

    const std::filesystem::path nested = writeProject(
        root / "nested", "ComprehensionNested", std::string(prelude) + R"NS(
public class Program
{
    public static int Main()
    {
        SumBuilder seed;
        seed.Total = 0;
        var result = comprehend(seed) {
            foreach (int left in 0..3) {
                foreach (int right in 0..3) {
                    if (left != right) yield left + right;
                }
            }
        };
        return result.Total == 12 ? 42 : 2;
    }
}
)NS");
    diagnostic.clear();
    const bool nestedAccepted = compileAndRun(nested, 42, diagnostic);
    tests.expect(nestedAccepted,
        "nested generators and contextual 'yield value' compose with var inference (" +
            diagnostic + ")");

    const std::filesystem::path early = writeProject(
        root / "early", "ComprehensionEarly", R"NS(
public copy struct SumBuilder
{
    public int Total;
    public noheap void Add(int value) { Total = Total + value; }
}
public class Program
{
    private static int Calls = 0;
    private static SumBuilder Seed()
    {
        Calls = Calls + 1;
        SumBuilder value;
        value.Total = 0;
        return value;
    }
    public static int Main()
    {
        SumBuilder result = comprehend(Seed()) {
            foreach (int value in 1..10) {
                if (value == 4) yield break;
                yield return value;
            }
        };
        return Calls == 1 && result.Total == 6 ? 42 : 3;
    }
}
)NS");
    diagnostic.clear();
    const bool earlyAccepted = compileAndRun(early, 42, diagnostic);
    tests.expect(earlyAccepted,
        "the explicit builder is evaluated once and yield break returns accumulated state (" +
            diagnostic + ")");

    const std::filesystem::path moveOnly = writeProject(
        root / "move-only", "ComprehensionMoveOnly", R"NS(
public struct MoveBuilder
{
    public int Total;
    public noheap void Add(int value) { Total = Total + value; }
}
public class Program
{
    public static noheap int Main()
    {
        MoveBuilder seed;
        seed.Total = 0;
        MoveBuilder result = comprehend(move(seed)) {
            foreach (int value in 1..=6) { yield return value; }
        };
        return result.Total == 21 ? 42 : 4;
    }
}
)NS");
    diagnostic.clear();
    const bool moveOnlyAccepted = compileAndRun(moveOnly, 42, diagnostic);
    tests.expect(moveOnlyAccepted,
        "an explicitly transferred move-only builder is returned without copying (" +
            diagnostic + ")");

    const std::filesystem::path missingAdd = writeProject(
        root / "missing-add", "ComprehensionMissingAdd", R"NS(
public struct Sink { public int Value; }
public class Program
{
    public static int Main()
    {
        Sink seed;
        Sink result = comprehend(seed) { yield return 42; };
        return result.Value;
    }
}
)NS");
    diagnostic.clear();
    const bool missingAddRejected = rejectedWith(missingAdd, "Add", diagnostic);
    tests.expect(missingAddRejected,
        "a builder without Add(value) is rejected by ordinary member lookup (" + diagnostic + ")");

    const std::filesystem::path edition2 = writeProject(
        root / "edition2", "ComprehensionEdition2", std::string(prelude) + R"NS(
public class Program
{
    public static int Main()
    {
        SumBuilder seed;
        SumBuilder result = comprehend(seed) { yield return 42; };
        return result.Total;
    }
}
)NS", 2);
    diagnostic.clear();
    const bool historicalEditionAccepted =
        !rejectedWith(edition2, "requires <Language>3</Language>", diagnostic);
    tests.expect(historicalEditionAccepted,
        "comprehensions are independent of historical edition input (" + diagnostic + ")");

    std::string mir;
    diagnostic.clear();
    const bool mirAccepted = dumpMir(filtered, mir, diagnostic);
    tests.expect(mirAccepted &&
            mir.find("new-object") == std::string::npos &&
            mir.find("array-new") == std::string::npos,
        "a stack builder comprehension introduces no implicit allocation (" + diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
