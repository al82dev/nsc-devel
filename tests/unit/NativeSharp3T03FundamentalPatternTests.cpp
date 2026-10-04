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
    writeProject(directory, "RejectedPattern", source);
    const std::string project = (directory / "RejectedPattern.nsproj").string();
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
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t03_fundamental_patterns";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    const std::filesystem::path direct = root / "direct";
    writeProject(direct, "FundamentalPatterns", R"NS(
namespace FundamentalPatterns
{
    public copy struct Point { public int X; public int Y; }
    public copy variant Payload
    {
        Deep((int, (int, int))),
        Position(Point),
        Number(int),
        Empty
    }
    public copy variant Choice { Left(int), Right(int), None }

    public class Program
    {
        public static int Main()
        {
            int score = 0;
            Payload deep = Payload.Deep((20, (1, 21)));
            match (deep)
            {
                Deep((20, (1, var tail))) => { score = score + tail; },
                _ => { return 1; }
            }

            Point point = { 20, 22 };
            match (Payload.Position(point))
            {
                Position({ X: 20, Y: var y }) => { score = score + y - 21; },
                _ => { return 2; }
            }

            match (5)
            {
                0..5 => { return 3; },
                5..10 => { score = score + 10; },
                _ => { return 4; }
            }

            match (10)
            {
                8 | 9 | 10 => { score = score + 10; },
                _ => { return 5; }
            }

            Choice right = Choice.Right(21);
            match (right)
            {
                Left(var merged) | Right(var merged) =>
                    { if (merged != 21) { return 7; } },
                _ => { return 8; }
            }
            Choice expressionChoice = Choice.Right(22);
            int expressionSingle = match (expressionChoice)
            {
                Right(var selected) => selected,
                _ => 0
            };
            if (expressionSingle != 22) { return 16; }
            int expressionMerged = match (expressionChoice)
            {
                Left(var merged) | Right(var merged) => merged,
                _ => 0
            };
            if (expressionMerged != 22) { return 9; }
            if (let Left(var merged) | Right(var merged) = Choice.Right(23))
            {
                if (merged != 23) { return 10; }
            }
            else { return 11; }
            let Left(var merged) | Right(var merged) = Choice.Left(24)
                else { return 12; };
            if (merged != 24) { return 13; }

            int aliased = 40;
            match (aliased)
            {
                ref value => { value = value + 2; }
            }
            if (aliased != 42) { return 6; }

            Point aliasPoint = { 20, 22 };
            match (aliasPoint)
            {
                { X: ref x, Y: _ } => { x = x + 1; },
                _ => { return 14; }
            }
            if (aliasPoint.X != 21) { return 15; }

            match (2)
            {
                move value => { score = score - value; }
            }
            return score + 2;
        }
    }
}
)NS");
    const std::string project = (direct / "FundamentalPatterns.nsproj").string();
    const std::string executable = (direct / "FundamentalPatterns.exe").string();
    const std::array<std::string_view, 5> emitArguments = {
        "--emit-project-oop-exe", project, "-O0", "-o", executable};
    std::string output;
    std::string error;
    const ExitCode emitResult = runDriver(emitArguments, output, error);
    tests.expect(
        emitResult == ExitCode::Success,
        "nested tuple/struct/variant, range, OR, value, ref and move patterns compile at O0 (" +
            error + output + ")");
    if (emitResult == ExitCode::Success)
    {
        const int executionResult = std::system(("\"" + executable + "\"").c_str());
        tests.expect(
            executionResult == 42,
            "fundamental patterns preserve decision order, bindings and ref aliasing (exit " +
                std::to_string(executionResult) + ")");
    }

    std::string diagnostic;
    tests.expect(
        rejected(root / "float", R"NS(
class Program { public static int Main() {
    match (1.0) { 1.0 => { return 1; }, _ => { return 0; } }
} }
)NS", "floating-point literals are not permitted in patterns", diagnostic),
        "floating literals receive the causal closed-domain diagnostic (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(
        rejected(root / "overlap", R"NS(
class Program { public static int Main() {
    match (7) { 0..10 => { return 1; }, 5..12 => { return 2; }, _ => { return 0; } }
} }
)NS", "overlaps an earlier alternative", diagnostic),
        "overlapping integer ranges are rejected deterministically (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(
        rejected(root / "or-bindings", R"NS(
variant Value { Some(int), None }
class Program { public static int Main() {
    match (Value.Some(1)) {
        Some(var value) | None => { return value; },
        _ => { return 0; }
    }
} }
)NS", "OR-pattern alternatives must bind identical names", diagnostic),
        "incompatible OR bindings are rejected before lowering (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(
        rejected(root / "partial-ref", R"NS(
variant Pair { Value((int, int)), None }
class Program { public static int Main() {
    Pair pair = Pair.Value((20, 22));
    match (pair) { Value((ref left, _)) => { return left; }, _ => { return 0; } }
} }
)NS", "cannot cross a tuple deconstruction or variant payload copy", diagnostic),
        "ref and move modes never pretend that a copied payload preserves aliasing (" +
            diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
