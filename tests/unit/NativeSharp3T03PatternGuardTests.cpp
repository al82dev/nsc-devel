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
    writeProject(directory, "RejectedGuard", source);
    const std::string project = (directory / "RejectedGuard.nsproj").string();
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
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t03_pattern_guards";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    const std::filesystem::path direct = root / "direct";
    writeProject(direct, "PatternGuards", R"NS(
namespace PatternGuards
{
    public variant Measurement { Value(int), Empty }

    public class Program
    {
        private static int GuardCalls;

        private static bool Positive(int value)
        {
            GuardCalls = GuardCalls + 1;
            return value > 0;
        }

        public static int Main()
        {
            int score = 0;
            match (Measurement.Value(41))
            {
                Value(var value) when value < 0 => { return 1; },
                Value(var value) when Positive(value) => { score = value; },
                Value(var value) => { return 2; },
                Empty => { return 3; }
            }
            if (GuardCalls != 1) return 4;

            match (Measurement.Empty())
            {
                Value(var value) when Positive(value) => { return 5; },
                Value(var value) => { return 6; },
                Empty => { score = score + 1; }
            }
            if (GuardCalls != 1) return 7;

            match (9)
            {
                0..10 when 9 < 0 => { return 8; },
                0..10 when true => { score = score + 0; },
                _ => { return 9; }
            }
            match (Measurement.Value(0))
            {
                Value(var value) when true => { score = score + value; },
                Empty => { return 10; }
            }
            match (1)
            {
                var value when value < 0 => { return 11; },
                _ => { score = score + 0; }
            }
            return score;
        }
    }
}
)NS");
    const std::string project = (direct / "PatternGuards.nsproj").string();
    const std::string executable = (direct / "PatternGuards.exe").string();
    const std::array<std::string_view, 5> emitArguments = {
        "--emit-project-oop-exe", project, "-O0", "-o", executable};
    std::string output;
    std::string error;
    const ExitCode emitResult = runDriver(emitArguments, output, error);
    tests.expect(
        emitResult == ExitCode::Success,
        "guards see pattern bindings and lower at O0 (" + error + output + ")");
    if (emitResult == ExitCode::Success)
    {
        tests.expect(
            std::system(("\"" + executable + "\"").c_str()) == 42,
            "guards run after structural matching, preserve order, and fall through on false");
    }

    std::string diagnostic;
    tests.expect(
        rejected(root / "coverage", R"NS(
variant Measurement { Value(int), Empty }
class Program { public static int Main() {
    match (Measurement.Value(1)) {
        Value(var value) when value > 0 => { return value; },
        Empty => { return 0; }
    }
} }
)NS", "missing variant case: Value", diagnostic),
        "a nonconstant guard does not make its variant case exhaustive (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(
        rejected(root / "open-domain", R"NS(
class Program { public static int Main() {
    match (1) { var value when value > 0 => { return value; } }
} }
)NS", "requires a final wildcard or binding arm", diagnostic),
        "a guarded catch-all does not close an open domain (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(
        rejected(root / "guard-type", R"NS(
class Program { public static int Main() {
    match (1) { var value when value => { return value; }, _ => { return 0; } }
} }
)NS", "if condition must have type bool", diagnostic),
        "a guard must be boolean (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(
        rejected(root / "constant-false", R"NS(
class Program { public static int Main() {
    match (1) { var value when false => { return value; }, _ => { return 0; } }
} }
)NS", "guard is the constant false", diagnostic),
        "a statically false guard is diagnosed as unreachable (" + diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
