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
    std::string_view source)
{
    const std::filesystem::path project = directory / (std::string(name) + ".nsproj");
    writeFile(
        project,
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><OutputType>Exe</OutputType></PropertyGroup>"
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

bool accepts(
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
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
    return compiled && !compilation.diagnostics().hasErrors();
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
public class Resource { }
public struct Token
{
    public int Value;
    public owned Resource Owner;
    public Token(int value) { Value = value; Owner = new Resource(); }
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
    tests.expect(accepts(root, "overlap-field", declarations + R"NS(
public class Program { public static int Main() { Pair value = new Pair(new Token(20), new Token(22)); ref int first = ref value.Left.Value; ref int second = ref value.Left.Value; return 0; } }
)NS", observed),
        "two mutable references may alias the same native field (" + observed + ")");

    observed.clear();
    tests.expect(accepts(root, "direct-access", declarations + R"NS(
public class Program { public static int Main() { Pair value = new Pair(new Token(20), new Token(22)); ref int alias = ref value.Left.Value; value.Left.Value = 42; return alias; } }
)NS", observed),
        "direct storage access remains valid while a native alias lives (" +
            observed + ")");

    observed.clear();
    tests.expect(accepts(root, "suspended-parent", R"NS(
public class Program { public static int Main() { int value = 1; ref int outer = ref value; { ref int inner = ref outer; outer = 2; } return 0; } }
)NS", observed),
        "a mutable reborrow and its parent remain ordinary native aliases (" +
            observed + ")");

    observed.clear();
    tests.expect(accepts(root, "same-index", R"NS(
public class Program { public static int Main() { int* values = stackalloc int[2]; ref int first = ref values[0]; ref int second = ref values[0]; return 0; } }
)NS", observed),
        "equal constant indices may produce native aliases (" + observed + ")");

    observed.clear();
    tests.expect(accepts(root, "dynamic-index", R"NS(
public class Program { public static int Main() { int* values = stackalloc int[2]; int a = 0; int b = 1; ref int first = ref values[a]; ref int second = ref values[b]; return 0; } }
)NS", observed),
        "dynamic indices do not impose an implicit exclusive-borrow policy (" + observed + ")");

    observed.clear();
    tests.expect(accepts(root, "indexer-fallback", R"NS(
public class Cell
{
    private int Value;
    public ref int this[int index] { get { return ref Value; } }
}
public class Program { public static int Main() { raii Cell cell = new Cell(); ref int first = ref cell[0]; ref int second = ref cell[1]; return 0; } }
)NS", observed),
        "user indexers may return aliases without implicit exclusivity (" +
            observed + ")");

    observed.clear();
    tests.expect(accepts(root, "unknown-call-origin", R"NS(
public class Program
{
    private static int Value;
    private static ref int Slot() { return ref Value; }
    public static int Main() { ref int first = ref Slot(); ref int second = ref Slot(); return 0; }
}
)NS", observed),
        "reference-return calls preserve native alias behavior even with unknown place provenance (" +
            observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "whole-move", declarations + R"NS(
public class Program { public static int Main() { Pair value = new Pair(new Token(20), new Token(22)); ref int alias = ref value.Left.Value; Pair moved = move(value); return alias + moved.Right.Value; } }
)NS", "while a borrowed local is alive", observed),
        "a whole-owner move remains forbidden while any mutable projection borrow lives (" +
            observed + ")");

    const std::filesystem::path compatibility = writeProject(
        root / "aliases", "ReferenceCompatibility", R"NS(
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
)NS");
    diagnostic.clear();
    tests.expect(compileAndRun(compatibility, 42, diagnostic),
        "ordinary references preserve their established native alias behavior (" + diagnostic + ")");

    return tests.resultKeeping(root);
}
