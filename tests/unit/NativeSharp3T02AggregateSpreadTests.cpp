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
using nativesharp::driver::Driver;
using nativesharp::ExitCode;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

void writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
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

bool rejected(
    const std::filesystem::path& directory,
    std::string_view source,
    std::string_view expected,
    std::string& diagnostic)
{
    writeProject(directory, "RejectedSpread", source);
    const std::string project = (directory / "RejectedSpread.nsproj").string();
    const std::array<std::string_view, 3> arguments = {
        "--check-project-oop-hir", project, "-O0"};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    return result != ExitCode::Success && diagnostic.find(expected) != std::string::npos;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t02_aggregate_spreads";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    writeProject(root / "positive", "AggregateSpreads", R"NS(
namespace AggregateSpreads
{
    public copy struct Pair { public int X; public int Y; }
    public copy struct Inner { public int Left; public int Right; }
    public copy struct Outer { public Inner InnerValue; public int Tail; }

    public copy struct Secret
    {
        private int Hidden;
        public int Visible;

        public static Secret Create(int hidden, int visible)
        {
            Secret result = { Hidden = hidden, Visible = visible };
            return result;
        }

        public int Sum() { return Hidden + Visible; }
    }

    public class Program
    {
        private static int trace;

        private static noheap int Mark(int digit, int value)
        {
            trace = trace * 10 + digit;
            return value;
        }

        private static noheap Pair Source()
        {
            trace = trace * 10 + 3;
            Pair result = { X = 20, Y = 40 };
            return result;
        }

        private static noheap Pair CopyNoHeap(Pair source)
        {
            Pair result = { ..source };
            return result;
        }

        public static int Main()
        {
            Pair source = { X = 1, Y = 2 };
            Pair overridden = { ..source, Y = 41 };
            if (overridden.X + overridden.Y != 42) { return 1; }
            if (source.X != 1 || source.Y != 2) { return 2; }

            trace = 0;
            Pair temporary = { X = Mark(2, 2), ..Source() };
            if (trace != 23 || temporary.X + temporary.Y != 42) { return 3; }

            Outer nestedSource = { InnerValue = { Left = 20, Right = 20 }, Tail = 1 };
            Outer nested = { Tail = 2, ..nestedSource };
            if (nested.InnerValue.Left + nested.InnerValue.Right + nested.Tail != 42) { return 4; }

            Secret secretSource = Secret.Create(20, 22);
            Secret secretCopy = { ..secretSource };
            if (secretCopy.Sum() != 42) { return 5; }

            Pair noHeapSource = { X = 20, Y = 22 };
            Pair noHeap = CopyNoHeap(noHeapSource);
            if (noHeap.X + noHeap.Y != 42) { return 6; }
            return 42;
        }
    }
}
)NS");

    const std::string project =
        (root / "positive" / "AggregateSpreads.nsproj").string();
    const std::string executable =
        (root / "positive" / "AggregateSpreads.exe").string();
    std::string output;
    std::string error;
    const std::array<std::string_view, 5> emitArguments = {
        "--emit-project-oop-exe", project, "-O0", "-o", executable};
    const ExitCode emitted = runDriver(emitArguments, output, error);
    tests.expect(
        emitted == ExitCode::Success,
        "O0 emits precedence, nesting, private-field copying, temporary source, and noheap spread (" +
            error + output + ")");
    if (emitted == ExitCode::Success)
    {
        tests.expect(
            std::system(("\"" + executable + "\"").c_str()) == 42,
            "aggregate spreads evaluate one source once and preserve explicit-field precedence");
    }

    const std::array<std::string_view, 3> hirArguments = {
        "--dump-project-oop-hir", project, "-O0"};
    output.clear();
    error.clear();
    tests.expect(
        runDriver(hirArguments, output, error) == ExitCode::Success &&
            output.find("aggregate-spread") != std::string::npos &&
            output.find("source-evaluations=1") != std::string::npos &&
            output.find("noheap=true") != std::string::npos,
        "HIR exposes one-evaluation spread and keeps noheap visible (" + error + output + ")");

    const std::array<std::string_view, 3> mirArguments = {
        "--dump-project-oop-mir", project, "-O0"};
    output.clear();
    error.clear();
    tests.expect(
        runDriver(mirArguments, output, error) == ExitCode::Success &&
            output.find("aggregate-spread-store") != std::string::npos &&
            output.find("source-evaluations=1") != std::string::npos,
        "MIR carries exact source/destination spread descriptors (" + error + output + ")");

    writeProject(root / "move_positive", "MoveAggregateSpread", R"NS(
public class Resource
{
    public static int Drops = 0;
    public int Value;
    public Resource(int value) { Value = value; }
    ~Resource() { Drops = Drops + 1; }
}

public struct Owner
{
    public owned Resource Left;
    public owned Resource Right;
}

public class Program
{
    public static int Main()
    {
        {
            Owner source = { new Resource(20), new Resource(20) };
            Owner changed = { Left = new Resource(22), ..move(source) };
            if (changed.Left.Value + changed.Right.Value != 42) { return 1; }
            if (source.Left.Value != 20) { return 2; }
        }
        if (Resource.Drops != 3) { return 3; }
        return 42;
    }
}
)NS");
    const std::string moveProject =
        (root / "move_positive" / "MoveAggregateSpread.nsproj").string();
    const std::string moveExecutable =
        (root / "move_positive" / "MoveAggregateSpread.exe").string();
    const std::array<std::string_view, 5> moveArguments = {
        "--emit-project-oop-exe", moveProject, "-O0", "-o", moveExecutable};
    output.clear();
    error.clear();
    const ExitCode moveEmitted = runDriver(moveArguments, output, error);
    tests.expect(
        moveEmitted == ExitCode::Success,
        "a move-spread transfers only unwritten owning fields at O0 through NSLink (" +
            error + output + ")");
    if (moveEmitted == ExitCode::Success)
    {
        tests.expect(
            std::system(("\"" + moveExecutable + "\"").c_str()) == 42,
            "move-spread keeps untouched siblings live and drops all three owners exactly once");
    }
    const std::array<std::string_view, 3> moveHirArguments = {
        "--dump-project-oop-hir", moveProject, "-O0"};
    output.clear();
    error.clear();
    tests.expect(
        runDriver(moveHirArguments, output, error) == ExitCode::Success &&
            output.find("aggregate-spread") != std::string::npos &&
            output.find("source-evaluations=1 mode=move") != std::string::npos,
        "HIR distinguishes a one-evaluation move-spread from a copy-spread (" +
            error + output + ")");
    const std::array<std::string_view, 3> moveMirArguments = {
        "--dump-project-oop-mir", moveProject, "-O0"};
    output.clear();
    error.clear();
    tests.expect(
        runDriver(moveMirArguments, output, error) == ExitCode::Success &&
            output.find("aggregate-spread-store") != std::string::npos &&
            output.find("source-evaluations=1 mode=move") != std::string::npos,
        "MIR preserves the move-spread transfer contract (" + error + output + ")");

    writeProject(root / "move_unwind", "MoveAggregateSpreadUnwind", R"NS(
namespace NativeSharp.Core { public class Exception { } }
using NativeSharp.Core;
public class Problem : Exception { }
public class Resource
{
    public static int Drops = 0;
    public Resource(int value) { }
    ~Resource() { Drops = Drops + 1; }
}
public struct Owner
{
    public owned Resource Left;
    public owned Resource Right;
}
public class Program
{
    private static owned Resource Fail() throws Problem { throw new Problem(); }
    public static int Main()
    {
        try
        {
            Owner source = { new Resource(20), new Resource(22) };
            Owner changed = { ..move(source), Left = Fail() };
            return 1;
        }
        catch (Problem problem) { }
        if (Resource.Drops != 2) { return 2; }
        return 42;
    }
}
)NS");
    const std::string unwindProject =
        (root / "move_unwind" / "MoveAggregateSpreadUnwind.nsproj").string();
    const std::string unwindExecutable =
        (root / "move_unwind" / "MoveAggregateSpreadUnwind.exe").string();
    const std::array<std::string_view, 5> unwindArguments = {
        "--emit-project-oop-exe", unwindProject, "-O0", "-o", unwindExecutable};
    output.clear();
    error.clear();
    const ExitCode unwindEmitted = runDriver(unwindArguments, output, error);
    tests.expect(
        unwindEmitted == ExitCode::Success,
        "move-spread initializer emits cleanup-aware unwind at O0 through NSLink (" +
            error + output + ")");
    if (unwindEmitted == ExitCode::Success)
    {
        tests.expect(
            std::system(("\"" + unwindExecutable + "\"").c_str()) == 42,
            "unwind drops the transferred destination field and untouched source field once");
    }

    std::string diagnostic;
    tests.expect(
        rejected(root / "two_sources", R"NS(
copy struct Pair { public int X; public int Y; }
class Program
{
    public static int Main()
    {
        Pair left = { 20, 22 };
        Pair right = { 21, 21 };
        Pair invalid = { ..left, ..right };
        return 0;
    }
}
)NS", "only one spread entry", diagnostic),
        "an initializer rejects a second spread source (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "duplicate", R"NS(
copy struct Pair { public int X; public int Y; }
class Program
{
    public static int Main()
    {
        Pair source = { 20, 22 };
        Pair invalid = { X = 1, X = 2, ..source };
        return 0;
    }
}
)NS", "assigned more than once", diagnostic),
        "explicit fields reject duplicate replacement before spread lowering (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "private", R"NS(
copy struct Secret { private int Hidden; public int Visible; }
class Program
{
    public static int Main()
    {
        Secret invalid = { Hidden = 20, Visible = 22 };
        return 0;
    }
}
)NS", "aggregate member 'Hidden' is not accessible", diagnostic),
        "explicit aggregate entries cannot bypass private visibility (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "private_positional", R"NS(
copy struct Secret { private int Hidden; public int Visible; }
class Program { public static int Main() { Secret invalid = { 20, 22 }; return 0; } }
)NS", "aggregate member 'Hidden' is not accessible", diagnostic),
        "positional aggregate entries cannot bypass private visibility (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "wrong_type", R"NS(
copy struct Left { public int X; public int Y; }
copy struct Right { public int X; public int Y; }
class Program
{
    public static int Main()
    {
        Right source = { 20, 22 };
        Left invalid = { ..source };
        return 0;
    }
}
)NS", "must have the same type", diagnostic),
        "spread source identity is exact (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "move_only", R"NS(
class Resource { }
struct Owner { public owned Resource Left; public owned Resource Right; }
class Program
{
    public static int Main()
    {
        Owner source = { null, null };
        Owner invalid = { Left = null, ..source };
        return 0;
    }
}
)NS", "requires '..move(source)'", diagnostic),
        "move-only spread rejects an implicit ownership copy (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "use_after_partial_move", R"NS(
class Resource { public int Value; public Resource(int value) { Value = value; } }
struct Owner { public owned Resource Left; public owned Resource Right; }
class Program
{
    public static int Main()
    {
        Owner source = { new Resource(20), new Resource(22) };
        Owner changed = { Left = null, ..move(source) };
        return source.Right.Value;
    }
}
)NS", "moved or possibly moved field path", diagnostic),
        "a field transferred by move-spread cannot be read from the partial source (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "owned_alias", R"NS(
class Resource { }
struct Owner { public owned Resource Left; public owned Resource Right; }
class Program
{
    public static int Main()
    {
        Owner source = { new Resource(), new Resource() };
        Owner invalid = { Left = source.Left, ..move(source) };
        return 0;
    }
}
)NS", "initializer for owned aggregate field 'Left' requires", diagnostic),
        "an explicit replacement cannot alias an owned field while the spread moves its sibling (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "custom_destructor", R"NS(
class Resource { }
struct Guard
{
    public owned Resource Value;
    ~Guard() { }
}
class Program
{
    public static int Main()
    {
        Guard source = { new Resource() };
        Guard invalid = { ..move(source) };
        return 0;
    }
}
)NS", "custom destructor requires the complete value", diagnostic),
        "move-spread rejects a source whose custom destructor may observe moved fields (" +
            diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
