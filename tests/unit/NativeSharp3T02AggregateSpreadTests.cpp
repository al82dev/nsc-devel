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
    public struct Pair { public int X; public int Y; }
    public struct Inner { public int Left; public int Right; }
    public struct Outer { public Inner InnerValue; public int Tail; }

    public struct Secret
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

    std::string diagnostic;
    tests.expect(
        rejected(root / "two_sources", R"NS(
struct Pair { public int X; public int Y; }
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
struct Pair { public int X; public int Y; }
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
struct Secret { private int Hidden; public int Visible; }
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
struct Secret { private int Hidden; public int Visible; }
class Program { public static int Main() { Secret invalid = { 20, 22 }; return 0; } }
)NS", "aggregate member 'Hidden' is not accessible", diagnostic),
        "positional aggregate entries cannot bypass private visibility (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "wrong_type", R"NS(
struct Left { public int X; public int Y; }
struct Right { public int X; public int Y; }
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
)NS", "requires a trivially destructible target", diagnostic),
        "owner-bearing partial spreads fail closed until T06 provides value drop flags (" +
            diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
