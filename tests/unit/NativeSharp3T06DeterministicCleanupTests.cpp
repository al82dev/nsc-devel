#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"

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
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::tests::TestContext;

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
    std::string_view edition = "3")
{
    const std::filesystem::path project = directory / (std::string(name) + ".nsproj");
    writeFile(project,
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>" + std::string(edition) +
        "</Language><OutputType>Exe</OutputType></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return project;
}

std::string diagnosticsText(const ProjectCompilation& compilation)
{
    std::string result;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (!result.empty()) result += " | ";
        result += diagnostic.message();
    }
    return result;
}

bool rejectsWith(
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    std::string_view expected,
    std::string& observed,
    std::string_view edition = "3")
{
    ProjectCompilation compilation;
    std::string error;
    std::string projectName(name);
    for (char& character : projectName)
        if (character == '-') character = '_';
    const bool compiled = compilation.compileProjectMethodBodies(
        writeProject(root / std::string(name), projectName, source, edition), error);
    observed = error + diagnosticsText(compilation);
    return (!compiled || compilation.diagnostics().hasErrors()) &&
        observed.find(expected) != std::string::npos;
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

bool compileAndRun(
    const std::filesystem::path& project,
    int expected,
    std::string& diagnostic)
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
    if (result != ExitCode::Success) return false;
    const int observed = std::system(("\"" + executableText + "\"").c_str());
    if (observed == expected) return true;
    diagnostic += "process exit code " + std::to_string(observed) +
        ", expected " + std::to_string(expected);
    return false;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t06_deterministic_cleanup";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    const std::filesystem::path positive = writeProject(
        root / "positive", "DeterministicCleanup", R"NS(
public class Token
{
    public int Value;
    public Token(int value) { Value = value; }
    ~Token() { Program.Record(9); }
}

public class Counter
{
    public int Value;
    public void Arm()
    {
        defer [ref this] { Value = 7; }
    }
}

public class Program
{
    private static int state;

    public static void Record(int digit) { state = state * 10 + digit; }

    private static void ReturnPath()
    {
        defer [] { Record(5); }
        return;
    }

    private static void LoopPath()
    {
        int index = 0;
        while (index < 3)
        {
            index = index + 1;
            defer [move index] { Record(index); }
            if (index == 1) continue;
            break;
        }
    }

    public static int Main()
    {
        state = 0;
        {
            int observed = 1;
            defer [ref observed] { Record(observed); }
            int snapshot = 3;
            defer [move snapshot] { Record(snapshot); }
            observed = 2;
        }
        ReturnPath();
        LoopPath();
        {
            raii Token token = new Token(4);
            defer [move token] { Record(token.Value); }
        }
        raii Counter counter = new Counter();
        counter.Arm();
        if (counter.Value != 7) return 1;
        return state == 3251249 ? 42 : 2;
    }
}
)NS");

    std::string diagnostic;
    const bool positiveRuns = compileAndRun(positive, 42, diagnostic);
    tests.expect(
        positiveRuns,
        "defer executes LIFO across fallthrough, return, continue, break and move-only destruction (" +
            diagnostic + ")");

    const std::string projectText = positive.string();
    const std::array<std::string_view, 3> dumpArguments = {
        "--dump-project-method-bodies", projectText, "-O0"};
    std::string output;
    std::string error;
    const bool dumpPasses =
        runDriver(dumpArguments, output, error) == ExitCode::Success &&
            output.find("kind=finally") != std::string::npos &&
            output.find("reason=return") != std::string::npos &&
            output.find("reason=break") != std::string::npos &&
            output.find("reason=continue") != std::string::npos;
    tests.expect(
        dumpPasses,
        "defer reuses the unified finally cleanup graph for every structured exit (" +
            error + output + ")");

    diagnostic.clear();
    const bool rejectsUncapturedLocal = rejectsWith(root, "uncaptured-local", R"NS(
public class Program
{
    public static int Main()
    {
        int value = 1;
        defer [] { value = value + 1; }
        return 42;
    }
}
)NS", "without an explicit ref or move capture", diagnostic);
    tests.expect(rejectsUncapturedLocal,
        "defer rejects an uncaptured outer local (" + diagnostic + ")");

    diagnostic.clear();
    const bool rejectsMovedOwner = rejectsWith(root, "moved-owner", R"NS(
public class Token { public int Value; public Token(int value) { Value = value; } }
public class Program
{
    public static int Main()
    {
        raii Token token = new Token(42);
        defer [move token] { int consumed = token.Value; }
        return token.Value;
    }
}
)NS", "moved-from", diagnostic);
    tests.expect(rejectsMovedOwner,
        "move capture transfers ownership at registration and rejects later source use (" +
            diagnostic + ")");

    diagnostic.clear();
    const bool rejectsUncapturedThis = rejectsWith(root, "uncaptured-this", R"NS(
public class Program
{
    private int value;
    public void Arm() { defer [] { value = 1; } }
    public static int Main() { return 42; }
}
)NS", "without an explicit 'ref this' capture", diagnostic);
    tests.expect(rejectsUncapturedThis,
        "defer rejects an implicit receiver without ref this (" + diagnostic + ")");

    diagnostic.clear();
    const bool rejectsDuplicate = rejectsWith(root, "duplicate-capture", R"NS(
public class Program
{
    public static int Main()
    {
        int value = 1;
        defer [ref value, move value] { }
        return 42;
    }
}
)NS", "duplicate defer capture", diagnostic);
    tests.expect(rejectsDuplicate,
        "defer rejects duplicate capture entries (" + diagnostic + ")");

    diagnostic.clear();
    const bool acceptsHistoricalEdition = !rejectsWith(root, "edition-two", R"NS(
public class Program
{
    public static int Main() { defer [] { } return 42; }
}
)NS", "requires <Language>3</Language>", diagnostic, "2");
    tests.expect(acceptsHistoricalEdition,
        "defer is independent of historical edition input (" + diagnostic + ")");

    return tests.resultKeeping(root);
}
