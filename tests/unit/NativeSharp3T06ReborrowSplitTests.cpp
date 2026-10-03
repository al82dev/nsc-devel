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
    writeFile(
        project,
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
    std::string& observed)
{
    ProjectCompilation compilation;
    std::string error;
    std::string projectName(name);
    for (char& character : projectName)
        if (character == '-') character = '_';
    const bool compiled = compilation.compileProjectOopMir(
        writeProject(root / std::string(name), projectName, source), error);
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
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t06_reborrow_split";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    const std::string declarations = R"NS(
public struct Token
{
    public int Value;
    public Token(int value) { Value = value; }
}
public struct Pair
{
    public Token Left;
    public Token Right;
    public Pair(Token left, Token right) { Left = move(left); Right = move(right); }
}
)NS";

    const std::filesystem::path positive = writeProject(
        root / "positive", "ReborrowSplitPositive", declarations + R"NS(
public class Program
{
    private static void Add(ref int target, int amount) { target = target + amount; }
    public static int Main()
    {
        Pair pair = new Pair(new Token(20), new Token(22));
        {
            ref int left = ref pair.Left.Value;
            Token extracted = move(pair.Right);
            Add(ref left, 1);
            if (left != 21 || extracted.Value != 22) { return 10; }
            pair.Right = new Token(0);
        }

        int* values = stackalloc int[2];
        values[0] = 19;
        values[1] = 21;
        {
            ref int first = ref values[0];
            ref int second = ref values[1];
            Add(ref first, 1);
            Add(ref second, 1);
        }
        if (values[0] != 20 || values[1] != 22) { return 11; }
        int index = 0;
        while (index < 2)
        {
            {
                ref int current = ref values[index];
                Add(ref current, 1);
            }
            index = index + 1;
        }
        if (values[0] != 21 || values[1] != 23) { return 13; }

        int answer = 0;
        {
            ref int outer = ref answer;
            {
                ref int inner = ref outer;
                inner = 42;
            }
            if (outer != 42) { return 12; }
        }
        return answer;
    }
}
)NS");
    std::string diagnostic;
    tests.expect(compileAndRun(positive, 42, diagnostic),
        "field siblings, constant array indices, calls and scoped mutable reborrow execute at O0 through NSLink (" +
            diagnostic + ")");

    const std::string projectText = positive.string();
    const std::array<std::string_view, 3> dumpArguments = {
        "--dump-project-method-bodies", projectText, "-O0"};
    std::string output;
    std::string error;
    tests.expect(
        runDriver(dumpArguments, output, error) == ExitCode::Success &&
            output.find("borrow-place=L") != std::string::npos &&
            output.find("reborrow-source-local=") != std::string::npos,
        "method-body dump publishes canonical mutable-borrow places and reborrow ancestry (" +
            error + output + ")");

    std::string observed;
    tests.expect(rejectsWith(root, "overlap-field", declarations + R"NS(
public class Program { public static int Main() { Pair value = new Pair(new Token(20), new Token(22)); ref int first = ref value.Left.Value; ref int second = ref value.Left.Value; return 0; } }
)NS", "exclusively borrows an overlapping place", observed),
        "two independent mutable references to the same field are rejected (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "direct-access", declarations + R"NS(
public class Program { public static int Main() { Pair value = new Pair(new Token(20), new Token(22)); ref int alias = ref value.Left.Value; value.Left.Value = 42; return alias; } }
)NS", "exclusively borrows an overlapping place", observed),
        "direct storage access is rejected while an overlapping mutable reference lives (" +
            observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "suspended-parent", R"NS(
public class Program { public static int Main() { int value = 1; ref int outer = ref value; { ref int inner = ref outer; outer = 2; } return 0; } }
)NS", "exclusively borrows an overlapping place", observed),
        "a mutable reborrow suspends access through its parent alias for the child scope (" +
            observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "same-index", R"NS(
public class Program { public static int Main() { int* values = stackalloc int[2]; ref int first = ref values[0]; ref int second = ref values[0]; return 0; } }
)NS", "exclusively borrows an overlapping place", observed),
        "equal constant indices overlap (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "dynamic-index", R"NS(
public class Program { public static int Main() { int* values = stackalloc int[2]; int a = 0; int b = 1; ref int first = ref values[a]; ref int second = ref values[b]; return 0; } }
)NS", "exclusively borrows an overlapping place", observed),
        "dynamic indices use the conservative overlapping fallback (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "indexer-fallback", R"NS(
public class Cell
{
    private int Value;
    public ref int this[int index] { get { return ref Value; } }
}
public class Program { public static int Main() { Cell cell = new Cell(); ref int first = ref cell[0]; ref int second = ref cell[1]; return 0; } }
)NS", "exclusively borrows an overlapping place", observed),
        "user indexers remain conservative because distinct written indices need not name disjoint storage (" +
            observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "unknown-call-origin", R"NS(
public class Program
{
    private static int Value;
    private static ref int Slot() { return ref Value; }
    public static int Main() { ref int first = ref Slot(); ref int second = ref Slot(); return 0; }
}
)NS", "already borrows an overlapping place", observed),
        "reference-return calls without comparable place provenance share the conservative unknown region (" +
            observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "whole-move", declarations + R"NS(
public class Program { public static int Main() { Pair value = new Pair(new Token(20), new Token(22)); ref int alias = ref value.Left.Value; Pair moved = move(value); return alias + moved.Right.Value; } }
)NS", "while a borrowed local is alive", observed),
        "a whole-owner move remains forbidden while any mutable projection borrow lives (" +
            observed + ")");

    const std::filesystem::path compatibility = writeProject(
        root / "edition2", "ReferenceCompatibility", R"NS(
public class Program
{
    public static int Main()
    {
        int value = 1;
        ref int first = ref value;
        ref int second = ref value;
        second = 42;
        return first;
    }
}
)NS", "2");
    diagnostic.clear();
    tests.expect(compileAndRun(compatibility, 42, diagnostic),
        "language edition 2 preserves its established reference-alias behavior (" + diagnostic + ")");

    return tests.resultKeeping(root);
}
