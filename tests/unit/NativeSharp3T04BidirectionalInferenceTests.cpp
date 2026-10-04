#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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

std::filesystem::path writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language><OutputType>Exe</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
        "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return directory / (std::string(name) + ".nsproj");
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
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source,
    int expected,
    std::string& diagnostic)
{
    const auto project = writeProject(directory, name, source);
    const std::string projectText = project.string();
    const std::string executableText = (directory / (std::string(name) + ".exe")).string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", projectText, "-O0", "-o", executableText};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    if (result != ExitCode::Success)
    {
        std::cerr << diagnostic << '\n';
        return false;
    }
    const int observed = std::system(("\"" + executableText + "\"").c_str());
    if (observed == expected) return true;
    diagnostic += "process exit code " + std::to_string(observed) +
        ", expected " + std::to_string(expected);
    std::cerr << diagnostic << '\n';
    return false;
}

bool rejected(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source,
    std::string_view expected,
    std::string& diagnostic)
{
    const auto project = writeProject(directory, name, source);
    const std::string projectText = project.string();
    const std::array<std::string_view, 3> arguments = {
        "--check-project-oop-hir", projectText, "-O0"};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    const bool accepted = result != ExitCode::Success &&
        diagnostic.find(expected) != std::string::npos;
    if (!accepted) std::cerr << diagnostic << '\n';
    return accepted;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t04_bidirectional_inference";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    std::string diagnostic;
    tests.expect(compileAndRun(root / "productive", "ProductiveInference", R"NS(
delegate int Mapper<T>(T value);
variant Maybe<T> { None, Some(T) }
struct Fixed<T, const int N>
{
    public T[N] Items;
    public int Count() { return N; }
}
class Program
{
    private static T DefaultOf<T>() { return default(T); }
    private static T Identity<T>(T value) { return value; }
    private static T Echo<T>(T value) { return value; }
    private static int Apply<T>(Mapper<T> mapper) { return mapper(default(T)); }
    private static int ApplyValue<T>(Mapper<T> mapper, T value) { return mapper(value); }
    private static int Increment(int value) { return value + 1; }
    private static Fixed<T, N> FixedDefault<T, const int N>() { return default(Fixed<T, N>); }
    private static T Stable<T>() { return default(T); }
    private static long Stable() { return 99L; }
    private static T Select<T>(T value) { return value; }
    private static T Select<T>(T[] values) { return values[0]; }

    public static int Main()
    {
        int nested = Identity(DefaultOf());
        var pair = Echo((20, 22));
        var (left, right) = pair;
        int lambda = Apply((int value) => value + 1);
        int methodGroup = ApplyValue(Increment, 40);
        Maybe<int> variant = DefaultOf();
        Fixed<int, 4> fixedValue = FixedDefault();
        int stable = Stable();
        int selected = Select(1);
        long explicitValue = Identity<long>(1);
        if (nested != 0 || left + right != 42 || lambda != 1 || methodGroup != 41 ||
            !variant.IsNone() || fixedValue.Count() != 4 || stable != 0 || selected != 1 ||
            explicitValue != 1L)
        {
            return 1;
        }
        return 42;
    }
}
)NS", 42, diagnostic),
        "exact expected types flow through nested calls, tuples, lambdas, method groups, variants, "
        "const parameters and irrelevant overloads in O0/NSLink (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(compileAndRun(root / "structural-context", "StructuralContext", R"NS(
class Program
{
    private static T DefaultOf<T>() { return default(T); }
    public static int Main()
    {
        (int, int) pair = (DefaultOf(), DefaultOf());
        int conditional = true ? DefaultOf() : DefaultOf();
        var (left, right) = pair;
        return left == 0 && right == 0 && conditional == 0 ? 42 : 1;
    }
}
)NS", 42, diagnostic),
        "one exact context propagates structurally into tuple elements and conditional branches (" +
        diagnostic + ")");

    diagnostic.clear();
    tests.expect(compileAndRun(root / "candidate-order", "CandidateOrder", R"NS(
class Program
{
    private static T Before<T>() { return default(T); }
    private static long Before() { return 1L; }
    private static long After() { return 2L; }
    private static T After<T>() { return default(T); }
    public static int Main()
    {
        int first = Before();
        int second = After();
        return first == 0 && second == 0 ? 42 : 1;
    }
}
)NS", 42, diagnostic),
        "an irrelevant non-generic candidate cannot change inference by declaration order (" +
        diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "conversion", "NoConversionGuess", R"NS(
class Program
{
    private static T Identity<T>(T value) { return value; }
    public static int Main() { long value = Identity(1); return (int)value; }
}
)NS", "write them explicitly", diagnostic),
        "expected-type inference does not guess through an integer conversion (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "conflict", "ConflictingEvidence", R"NS(
class Program
{
    private static T Same<T>(T left, T right) { return left; }
    public static int Main() { return Same(1, 2L); }
}
)NS", "cannot be inferred", diagnostic),
        "conflicting evidence is deterministic and requires explicit arguments (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "lambda-ambiguity", "LambdaAmbiguity", R"NS(
delegate int First(int value);
delegate int Second(int value);
class Program
{
    private static int Use<T>(First callback, T value) { return callback(value); }
    private static int Use<T>(Second callback, T value) { return callback(value); }
    public static int Main() { return Use(value => value, 42); }
}
)NS", "requires an explicit type for every parameter", diagnostic),
        "an untyped lambda with two exact delegate targets remains ambiguous (" + diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
