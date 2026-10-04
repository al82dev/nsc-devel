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
    writeProject(directory, "Rejected", source);
    const std::string project = (directory / "Rejected.nsproj").string();
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
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t01_generalized_initializers";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    const std::filesystem::path positive = root / "positive";
    writeProject(positive, "GeneralizedInitializers", R"NS(
namespace GeneralizedInitializers
{
    public readonly struct ReadonlyPair
    {
        public readonly int X;
        public readonly int Y;
    }

    public struct Pair<T>
    {
        public T First;
        public T Second;
    }

    public struct Nested
    {
        public ReadonlyPair Inner;
        public int Tail;
    }

    public class Program
    {
        private static int trace = 0;
        private static int Mark(int digit, int value)
        {
            trace = trace * 10 + digit;
            return value;
        }

        public static int Main()
        {
            ReadonlyPair readOnly = { Y = Mark(1, 22), X = Mark(2, 20) };
            if (readOnly.X + readOnly.Y != 42 || trace != 12) { return 1; }

            Pair<int> source = { First = 10, Second = 32 };
            trace = 0;
            Pair<int> spread = { Second = Mark(3, 22), ..source };
            if (spread.First + spread.Second != 32 || trace != 3) { return 2; }

            Nested nested = { Inner = { X = 20, Y = 20 }, Tail = 2 };
            if (nested.Inner.X + nested.Inner.Y + nested.Tail != 42) { return 3; }

            int[4] fixedValues = { 10, 20, 9, 3 };
            int[4] fixedCopy = { ..fixedValues };
            if (fixedCopy[0] + fixedCopy[1] + fixedCopy[2] + fixedCopy[3] != 42)
                { return 4; }
            return 42;
        }
    }
}
)NS");

    const std::string positiveProject = (positive / "GeneralizedInitializers.nsproj").string();
    const std::string executable = (positive / "GeneralizedInitializers.exe").string();
    const std::array<std::string_view, 5> emitArguments = {
        "--emit-project-oop-exe", positiveProject, "-O0", "-o", executable};
    std::string output;
    std::string error;
    const ExitCode emitted = runDriver(emitArguments, output, error);
    tests.expect(
        emitted == ExitCode::Success,
        "O0 compiles readonly, generic, nested, spread and fixed-array initializers (" +
            error + output + ")");
    if (emitted == ExitCode::Success)
    {
        tests.expect(
            std::system(("\"" + executable + "\"").c_str()) == 42,
            "generalized initializers preserve textual evaluation and values");
    }

    const std::array<std::string_view, 3> hirArguments = {
        "--dump-project-oop-hir", positiveProject, "-O0"};
    output.clear();
    error.clear();
    tests.expect(
        runDriver(hirArguments, output, error) == ExitCode::Success &&
            output.find("aggregate-spread") != std::string::npos,
        "HIR exposes one aggregate-spread operation (" + error + output + ")");

    const std::array<std::string_view, 3> llvmArguments = {
        "--dump-project-oop-llvm", positiveProject, "-O0"};
    output.clear();
    error.clear();
    tests.expect(
        runDriver(llvmArguments, output, error) == ExitCode::Success &&
            output.find("aggregate.init") == std::string::npos &&
            output.find("aggregate.spread.source") != std::string::npos,
        "local aggregates initialize their final slot without aggregate.init temporaries (" +
            error + ")");

    std::string diagnostic;
    tests.expect(
        rejected(root / "duplicate", R"NS(
struct Pair { public int X; public int Y; }
class Program { public static int Main() { Pair value = { X = 1, X = 2, Y = 39 }; return 0; } }
)NS", "assigned more than once", diagnostic),
        "duplicate named members are rejected deterministically (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "missing", R"NS(
struct Pair { public int X; public int Y; }
class Program { public static int Main() { Pair value = { X = 42 }; return value.X; } }
)NS", "leaves 1 member(s) uninitialized", diagnostic),
        "generalized forms cannot omit members without an explicit spread (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "destructor", R"NS(
struct Guard { public int Value; ~Guard() { } }
class Program { public static int Main() { Guard value = { Value = 42 }; return value.Value; } }
)NS", "requires exactly one '..move(source)' spread", diagnostic),
        "a destructor-bearing move-only value cannot use a generalized initializer without an "
        "explicit move spread (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "noncopyable", R"NS(
class Resource { }
struct Owner { public owned Resource Value; }
class Program
{
    public static int Main()
    {
        Owner source = { null };
        Owner copy = { ..source };
        return 0;
    }
}
)NS", "requires '..move(source)'", diagnostic),
        "noncopyable owner-bearing spreads require explicit ownership transfer before lowering (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "wrong-spread", R"NS(
struct Left { public int X; public int Y; }
struct Right { public int X; public int Y; }
class Program
{
    public static int Main()
    {
        Right source = { 20, 22 };
        Left value = { ..source };
        return value.X + value.Y;
    }
}
)NS", "must have the same type", diagnostic),
        "spread sources require exact aggregate identity (" + diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
