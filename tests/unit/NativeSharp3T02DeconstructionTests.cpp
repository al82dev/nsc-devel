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
    writeProject(directory, "RejectedDeconstruction", source);
    const std::string project = (directory / "RejectedDeconstruction.nsproj").string();
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
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t02_deconstruction";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    writeProject(root / "positive", "Deconstruction", R"NS(
namespace Deconstruction
{
    public struct Pair
    {
        public int X;
        public int Y;
        public int Calls;

        public (int, int) Deconstruct()
        {
            Calls = Calls + 1;
            return (X, Y);
        }
    }

    public struct Nested
    {
        public int A;
        public int B;
        public int C;
        public readonly (int, (int, int)) Deconstruct() { return (A, (B, C)); }
    }

    public class Program
    {
        private static (int, int) Shared;
        private static int nextSlot;
        private static int writeTrace;

        private static ref (int, int) SharedRef() { return ref Shared; }

        private static int NextSlot()
        {
            int result = nextSlot;
            nextSlot = nextSlot + 1;
            writeTrace = writeTrace * 10 + nextSlot;
            return result;
        }

        public static int Main()
        {
            var () = ();
            var (only,) = (42,);
            if (only != 42) { return 1; }

            var (a, (b, c)) = (1, (2, 3));
            if (a != 1 || b != 2 || c != 3) { return 2; }
            (a, (b, c)) = (4, (5, 6));
            if (a != 4 || b != 5 || c != 6) { return 3; }

            int left = 20;
            int right = 22;
            (left, right) = (right, left);
            if (left != 22 || right != 20) { return 4; }
            (_, right) = (99, 22);
            if (right != 22) { return 5; }

            Pair pair = { 20, 22, 0 };
            var (px, py) = pair;
            if (px + py != 42 || pair.Calls != 1) { return 6; }

            Nested nested = { 20, 10, 12 };
            var (nx, (ny, nz)) = nested;
            if (nx + ny + nz != 42) { return 7; }

            Shared = (20, 22);
            var (rx, ry) = SharedRef();
            if (rx + ry != 42) { return 8; }

            int[2] values;
            values[0] = 20;
            values[1] = 22;
            nextSlot = 0;
            writeTrace = 0;
            (values[NextSlot()], values[NextSlot()]) = (values[1], values[0]);
            if (writeTrace != 12 || values[0] != 22 || values[1] != 20) { return 9; }
            return 42;
        }
    }
}
)NS");

    const std::string project = (root / "positive" / "Deconstruction.nsproj").string();
    const std::string executable = (root / "positive" / "Deconstruction.exe").string();
    const std::array<std::string_view, 5> emitArguments = {
        "--emit-project-oop-exe", project, "-O0", "-o", executable};
    std::string output;
    std::string error;
    const ExitCode emitted = runDriver(emitArguments, output, error);
    tests.expect(
        emitted == ExitCode::Success,
        "O0 compiles unit/unary, nested, nominal and ref-source deconstruction (" +
            error + output + ")");
    if (emitted == ExitCode::Success)
    {
        tests.expect(
            std::system(("\"" + executable + "\"").c_str()) == 42,
            "deconstruction snapshots once and writes target leaves left-to-right");
    }

    std::string diagnostic;
    tests.expect(
        rejected(root / "nested_conversion", R"NS(
class Program
{
    public static int Main()
    {
        int x = 0;
        int y = 0;
        (x, (y, x)) = (1, ("bad", 2));
        return 0;
    }
}
)NS", "deconstruction element 1.0", diagnostic),
        "a nested conversion failure identifies the exact leaf before lowering (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "invalid_protocol", R"NS(
struct Bad
{
    public (int, int) Deconstruct(int ignored) { return (20, 22); }
}
class Program
{
    public static int Main()
    {
        Bad value;
        var (x, y) = value;
        return 0;
    }
}
)NS", "parameterless", diagnostic),
        "the nominal protocol rejects a Deconstruct overload with the wrong signature (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "ref_protocol", R"NS(
struct Bad
{
    public (int, int) Value;
    public ref (int, int) Deconstruct() { return ref Value; }
}
class Program
{
    public static int Main()
    {
        Bad value;
        var (x, y) = value;
        return 0;
    }
}
)NS", "returning a tuple by value", diagnostic),
        "the nominal protocol rejects a ref-returning Deconstruct method (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "move_only_temporary", R"NS(
struct Owner
{
    public owned string Text;
    public (int,) Deconstruct() { return (42,); }
}
class Program
{
    private static Owner Make()
    {
        Owner value = { "owned" };
        return move(value);
    }
    public static int Main()
    {
        var (answer,) = Make();
        return answer;
    }
}
)NS", "hidden copy", diagnostic),
        "a hidden temporary cannot copy a move-only nominal source (" + diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
