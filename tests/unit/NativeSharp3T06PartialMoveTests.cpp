#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"

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
        "</Name><Language>3</Language><OutputType>Exe</OutputType></PropertyGroup>"
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
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t06_partial_moves";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    const std::filesystem::path positive = writeProject(root / "positive", "PartialMovePositive", R"NS(
public class Resource
{
    public static int Drops = 0;
    public int Value;
    public Resource(int value) { Value = value; }
    ~Resource() { Drops = Drops + 1; }
}
public struct Token
{
    public owned Resource ResourceValue;
    public Token(owned Resource value) { ResourceValue = move(value); }
}
public struct Pair
{
    public Token Left;
    public Token Right;
    public Pair(Token left, Token right) { Left = move(left); Right = move(right); }
}
public class Program
{
    public static int Main()
    {
        int answer = 0;
        {
            Pair pair = new Pair(new Token(new Resource(20)), new Token(new Resource(22)));
            Token extracted = move(pair.Left);
            answer = extracted.ResourceValue.Value + pair.Right.ResourceValue.Value;
            pair.Left = new Token(new Resource(0));
        }
        if (Resource.Drops != 3) { return 10; }
        return answer;
    }
}
)NS");
    std::string diagnostic;
    tests.expect(compileAndRun(positive, 42, diagnostic),
        "a moved field, live sibling and reinitialized field execute with exact recursive drops at O0 through NSLink (" +
            diagnostic + ")");

    const std::filesystem::path destructor = writeProject(
        root / "destructor", "ValueDestructor", R"NS(
namespace NativeSharp.Core { public class Exception { } }
using NativeSharp.Core;
public class Problem : Exception { }
public struct Guard
{
    public static int Drops = 0;
    public int Value;
    public Guard(int value) { Value = value; }
    ~Guard() { Drops = Drops + 1; }
}
public class Program
{
    private static int Consume(Guard value)
    {
        if (value.Value == 20) { return 20; }
        return 0;
    }
    private static int EarlyReturn()
    {
        Guard local = new Guard(22);
        return local.Value;
    }
    private static void Throwing() throws Problem
    {
        Guard local = new Guard(7);
        throw new Problem();
    }
    public static int Main()
    {
        { Guard first = new Guard(42); Guard second = move(first); }
        if (Guard.Drops != 1) { return 11; }
        if (Consume(new Guard(20)) != 20 || Guard.Drops != 2) { return 12; }
        if (EarlyReturn() != 22 || Guard.Drops != 3) { return 13; }
        try { Throwing(); return 14; }
        catch (Problem problem) { }
        if (Guard.Drops != 4) { return 15; }
        return 42;
    }
}
)NS");
    diagnostic.clear();
    tests.expect(compileAndRun(destructor, 42, diagnostic),
        "whole moves, value parameters, early returns and native unwind execute each custom "
        "destructor exactly once (" + diagnostic + ")");

    const std::filesystem::path genericOptional = writeProject(
        root / "generic-optional", "GenericOptionalPartialMove", R"NS(
namespace NativeSharp.Core
{
    public struct Optional<T>
    {
        private T value;
        private bool present;

        public Optional(T item)
        {
            value = move(item);
            present = true;
        }

        public bool HasValue()
        {
            return present;
        }

        public T UncheckedValue()
        {
            return move(value);
        }
    }
}

using NativeSharp.Core;

public class Resource
{
    public static int Drops = 0;
    public int Value;
    public Resource(int value) { Value = value; }
    ~Resource() { Drops = Drops + 1; }
}

public struct Payload
{
    public owned Resource ResourceValue;
    public Payload(owned Resource value) { ResourceValue = move(value); }
}

public class Program
{
    private static Optional<Payload> Source(bool present, int value)
    {
        if (present)
        {
            return new Optional<Payload>(new Payload(new Resource(value)));
        }
        return new Optional<Payload>();
    }

    public static int Main()
    {
        int answer = 0;
        {
            Payload first = Source(true, 20) ?? new Payload(new Resource(90));
            Payload second = Source(false, 0) ?? new Payload(new Resource(22));
            answer = first.ResourceValue.Value + second.ResourceValue.Value;
        }
        if (Resource.Drops != 2) { return 17; }
        return answer;
    }
}
)NS");
    diagnostic.clear();
    tests.expect(compileAndRun(genericOptional, 42, diagnostic),
        "Optional<T> extracts and returns a move-only generic field while exact drop state "
        "destroys each selected payload once (" + diagnostic + ")");

    const std::filesystem::path borrowedGenericField = writeProject(
        root / "borrowed-generic-field", "BorrowedGenericField", R"NS(
public class Source<T>
{
    public int Value;
    public Source(int value) { Value = value; }
}
public class Enumerator<T>
{
    private Source<T> source;
    public Enumerator(Source<T> value) { source = value; }
    public int Current() { return source.Value; }
}
public class Program
{
    public static int Main()
    {
        Source<int> source = new Source<int>(42);
        Enumerator<int> iterator = new Enumerator<int>(source);
        return iterator.Current();
    }
}
)NS");
    diagnostic.clear();
    tests.expect(compileAndRun(borrowedGenericField, 42, diagnostic),
        "a plain reference field in a generic class remains borrowed and accepts ordinary "
        "assignment without an ownership transfer (" + diagnostic + ")");

    const std::string declarations = R"NS(
public struct Token { public int Value; public Token(int value) { Value = value; } }
public struct Pair { public Token Left; public Token Right; public Pair(Token left, Token right) { Left = move(left); Right = move(right); } }
)NS";
    std::string observed;
    tests.expect(rejectsWith(root, "whole-after-partial", declarations + R"NS(
public class Program { public static int Main() { Pair value = new Pair(new Token(20), new Token(22)); Token left = move(value.Left); Pair all = move(value); return left.Value; } }
)NS", "after one of its fields was moved", observed),
        "the complete aggregate cannot move after a field move (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "double-field-move", declarations + R"NS(
public class Program { public static int Main() { Pair value = new Pair(new Token(20), new Token(22)); Token a = move(value.Left); Token b = move(value.Left); return a.Value + b.Value; } }
)NS", "already moved field path", observed),
        "the same field cannot be moved twice (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "branch-field-move", declarations + R"NS(
public class Program { public static int Main() { Pair value = new Pair(new Token(20), new Token(22)); if (value.Right.Value == 22) { Token a = move(value.Left); } return value.Left.Value; } }
)NS", "possibly moved field path", observed),
        "a branch join rejects a possibly moved field read (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "loop-field-move", declarations + R"NS(
public class Program { public static int Main() { Pair value = new Pair(new Token(20), new Token(22)); bool again = true; while (again) { Token a = move(value.Left); again = false; } return value.Right.Value; } }
)NS", "back-edge", observed),
        "a partial move cannot remain live across a loop back-edge (" + observed + ")");

    observed.clear();
    tests.expect(rejectsWith(root, "destructor-partial", R"NS(
public struct Token { public int Value; public Token(int value) { Value = value; } }
public struct Guard { public Token Value; public Guard(Token value) { Value = move(value); } ~Guard() { } }
public class Program { public static int Main() { Guard value = new Guard(new Token(42)); Token token = move(value.Value); return token.Value; } }
)NS", "custom destructor requires the complete value", observed),
        "partial moves are rejected when a custom destructor observes the complete value (" + observed + ")");

    return tests.resultKeeping(root);
}
