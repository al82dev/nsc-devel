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
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t01_with_expressions";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    const std::filesystem::path positive = root / "positive";
    writeProject(positive, "WithExpressions", R"NS(
namespace WithExpressions
{
    public readonly copy struct ReadonlyPair
    {
        public readonly int X;
        public readonly int Y;
    }

    public copy struct Pair<T> where T : copy
    {
        public T First;
        public T Second;
    }

    public copy struct Vault
    {
        private int Secret;
        public int Visible;

        public static Vault Create(int secret, int visible)
        {
            Vault value = { secret, visible };
            return value;
        }

        public static Vault ChangeSecret(Vault source, int secret)
        {
            return source with { Secret = secret };
        }

        public int ReadSecret() { return Secret; }
    }

    public struct CloneBox
    {
        public int Value;
        public CloneBox Clone()
        {
            CloneBox result = { Value };
            return move(result);
        }
    }

    public class Program
    {
        private static int trace = 0;
        private static int Mark(int digit, int value)
        {
            trace = trace * 10 + digit;
            return value;
        }

        private static Pair<int> Source()
        {
            trace = trace * 10 + 1;
            Pair<int> value = { 10, 32 };
            return value;
        }

        private static noheap ReadonlyPair ChangeReadonly(ReadonlyPair source)
        {
            return source with { Y = 22, X = 20 };
        }

        public static int Main()
        {
            trace = 0;
            Pair<int> changed = Source() with {
                Second = Mark(2, 20), First = Mark(3, 22) };
            if (trace != 123 || changed.First + changed.Second != 42) { return 1; }

            ReadonlyPair initial = { X = 1, Y = 2 };
            ReadonlyPair readOnly = ChangeReadonly(initial);
            if (readOnly.X + readOnly.Y != 42) { return 2; }

            Vault vault = Vault.Create(1, 2);
            Vault privateChanged = Vault.ChangeSecret(vault, 40);
            if (privateChanged.ReadSecret() + privateChanged.Visible != 42) { return 3; }

            CloneBox box = { 1 };
            CloneBox cloned = box.Clone() with { Value = 42 };
            if (box.Value != 1 || cloned.Value != 42) { return 4; }
            return 42;
        }
    }
}
)NS");

    const std::string project = (positive / "WithExpressions.nsproj").string();
    const std::string executable = (positive / "WithExpressions.exe").string();
    const std::array<std::string_view, 5> emitArguments = {
        "--emit-project-oop-exe", project, "-O0", "-o", executable};
    std::string output;
    std::string error;
    const ExitCode emitted = runDriver(emitArguments, output, error);
    tests.expect(
        emitted == ExitCode::Success,
        "O0 compiles POD, generic, readonly, private-in-scope and explicit Clone sources (" +
            error + output + ")");
    if (emitted == ExitCode::Success)
    {
        tests.expect(
            std::system(("\"" + executable + "\"").c_str()) == 42,
            "with evaluates source once, applies updates in text order, and preserves source");
    }

    const std::array<std::string_view, 3> hirArguments = {
        "--dump-project-oop-hir", project, "-O0"};
    output.clear();
    error.clear();
    tests.expect(
        runDriver(hirArguments, output, error) == ExitCode::Success &&
            output.find("aggregate-spread") != std::string::npos,
        "HIR exposes whole-value copy as aggregate-spread without a wrapper layout (" +
            error + output + ")");

    std::string diagnostic;
    tests.expect(
        rejected(root / "private", R"NS(
struct Vault
{
    private int Secret;
    public int Visible;
    public static Vault Create() { Vault value = { 1, 2 }; return value; }
}
class Program
{
    public static int Main()
    {
        Vault source = Vault.Create();
        Vault changed = source with { Secret = 42 };
        return 0;
    }
}
)NS", "with member 'Secret' is not accessible", diagnostic),
        "with cannot bypass private-field accessibility (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "duplicate", R"NS(
struct Pair { public int X; public int Y; }
class Program
{
    public static int Main()
    {
        Pair source = { 1, 2 };
        Pair changed = source with { X = 20, X = 22 };
        return 0;
    }
}
)NS", "assigned more than once", diagnostic),
        "duplicate with updates are rejected deterministically (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "owning", R"NS(
struct Box { public owned string Text; }
class Program
{
    public static int Main()
    {
        Box source = { "before" };
        Box changed = source with { Text = "after" };
        return 0;
    }
}
)NS", "explicit clone/copy strategy", diagnostic),
        "move-only or owner-bearing values fail before an aliasing copy can be lowered (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "class", R"NS(
class Box { public int Value; }
class Program
{
    public static int Main()
    {
        raii Box source = new Box();
        Box changed = source with { Value = 42 };
        return 0;
    }
}
)NS", "requires a named struct value", diagnostic),
        "reference identity is never copied implicitly by with (" + diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
