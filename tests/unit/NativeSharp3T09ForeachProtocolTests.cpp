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

std::string frontendDiagnostics(const std::filesystem::path& project)
{
    ProjectCompilation compilation;
    std::string error;
    const bool accepted = compilation.compileProjectMethodBodies(project, error);
    std::string result = "frontend accepted=" + std::string(accepted ? "true" : "false") +
        ", diagnostics=" + std::to_string(compilation.diagnostics().diagnostics().size()) + "\n" + error;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
        result += diagnostic.message() + "\n";
    return result;
}

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

std::filesystem::path writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source,
    int edition = 3)
{
    const std::filesystem::path project = directory / (std::string(name) + ".nsproj");
    writeFile(project,
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>" + std::to_string(edition) +
        "</Language><OutputType>Exe</OutputType></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return project;
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
    if (result != ExitCode::Success)
    {
        diagnostic += "driver exit=" + std::to_string(static_cast<int>(result)) + "\n";
        diagnostic += frontendDiagnostics(project);
        return false;
    }
    const int observed = std::system(("\"" + executableText + "\"").c_str());
    if (observed == expected) return true;
    diagnostic += "process exit code " + std::to_string(observed) +
        ", expected " + std::to_string(expected);
    return false;
}

bool rejectedWith(
    const std::filesystem::path& project,
    std::string_view expected,
    std::string& diagnostic)
{
    const std::string projectText = project.string();
    const std::array<std::string_view, 3> arguments = {
        "--dump-project-method-bodies", projectText, "-O0"};
    std::string output;
    std::string error;
    const bool accepted = runDriver(arguments, output, error) == ExitCode::Success;
    diagnostic = error + output;
    if (!accepted) diagnostic += frontendDiagnostics(project);
    else diagnostic += "driver accepted project\n";
    return !accepted && diagnostic.find(expected) != std::string::npos;
}

bool dumpMir(
    const std::filesystem::path& project,
    std::string& output,
    std::string& diagnostic)
{
    const std::string projectText = project.string();
    const std::array<std::string_view, 3> arguments = {
        "--dump-project-oop-mir", projectText, "-O0"};
    std::string error;
    const bool accepted = runDriver(arguments, output, error) == ExitCode::Success;
    diagnostic = error;
    if (!accepted) diagnostic += frontendDiagnostics(project);
    return accepted;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3-t09-f01-directed";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) /
            "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    const std::filesystem::path references = writeProject(
        root / "references", "ForeachReferences", R"NS(
namespace ForeachReferences
{
    public class Program
    {
        public static int Main()
        {
            int[] values = new int[3];
            values[0] = 10;
            values[1] = 11;
            values[2] = 18;
            foreach (ref mut int value in values) { value = value + 1; }
            int sum = 0;
            foreach (ref int value in values) { sum = sum + value; }
            return sum;
        }
    }
}
)NS");
    std::string diagnostic;
    tests.expect(compileAndRun(references, 42, diagnostic),
        "O0/NSLink executes explicit ref mut writes and readonly ref reads over arrays (" +
            diagnostic + ")");

    const std::filesystem::path valueSnapshot = writeProject(
        root / "value", "ForeachValues", R"NS(
namespace ForeachValues
{
    public class Program
    {
        public static int Main()
        {
            int[] values = new int[1];
            values[0] = 42;
            int result = 0;
            foreach (int value in values) { result = value; }
            return result;
        }
    }
}
)NS");
    diagnostic.clear();
    tests.expect(compileAndRun(valueSnapshot, 42, diagnostic),
        "value iteration remains the allocation-free default (" + diagnostic + ")");

    const std::filesystem::path readonly = writeProject(
        root / "readonly", "ForeachReadonly", R"NS(
namespace ForeachReadonly
{
    public class Program
    {
        public static int Main()
        {
            int[] values = new int[1];
            foreach (ref int value in values) { value = 42; }
            return 0;
        }
    }
}
)NS");
    diagnostic.clear();
    tests.expect(rejectedWith(readonly, "left side of assignment is not assignable", diagnostic),
        "readonly foreach ref cannot mutate its aliased element (" + diagnostic + ")");

    const std::filesystem::path span = writeProject(
        root / "span", "ForeachSpan", R"NS(
using NativeSharp.Core;
namespace ForeachSpan
{
    public class Program
    {
        public static int Main()
        {
            int* raw = stackalloc int[3];
            raw[0] = 10;
            raw[1] = 11;
            raw[2] = 18;
            Span<int> values = new Span<int>(raw, 3);
            foreach (ref mut int value in values) { value = value + 1; }
            int sum = 0;
            foreach (ref int value in values) { sum = sum + value; }
            return sum;
        }
    }
}
)NS");
    writeFile(root / "span" / "Span.ns", R"NS(
namespace NativeSharp.Core
{
    public borrowed struct Span<T>
    {
        private T* start;
        private int count;
        public Span(T* data, int length) { start = data; count = length; }
        public int Length() { return count; }
        public T* Data() { return start; }
    }
}
)NS");
    diagnostic.clear();
    const bool spanExecuted = compileAndRun(span, 42, diagnostic);
    tests.expect(spanExecuted,
        "O0/NSLink iterates Span<T> by value/ref through its stable Data storage (" +
            diagnostic + ")");
    std::string mir;
    diagnostic.clear();
    const bool spanMir = dumpMir(span, mir, diagnostic);
    tests.expect(spanMir && mir.find("raw-element-address") != std::string::npos &&
            mir.find("array-element-address") == std::string::npos,
        "Span foreach lowers through raw element addresses without managed bounds guards (" +
            diagnostic + ")");

    const std::filesystem::path moveOnly = writeProject(
        root / "move-only", "ForeachMoveOnly", R"NS(
namespace ForeachMoveOnly
{
    public struct Guard
    {
        public int value;
        ~Guard() { value = 0; }
    }
    public class Program
    {
        public static int Main()
        {
            Guard[] values = new Guard[1];
            foreach (Guard value in values) { return value.value; }
            return 0;
        }
    }
}
)NS");
    diagnostic.clear();
    const bool moveOnlyRejected =
        rejectedWith(moveOnly, "move-only value", diagnostic);
    tests.expect(moveOnlyRejected,
        "value foreach cannot silently copy a move-only element; ref mode is required (" +
            diagnostic + ")");

    const std::filesystem::path rangeRef = writeProject(
        root / "range-ref", "ForeachRangeRef", R"NS(
class Program
{
    public static int Main()
    {
        foreach (ref mut int value in 0..4) { value = 42; }
        return 0;
    }
}
)NS");
    diagnostic.clear();
    const bool rangeRefRejected = rejectedWith(
        rangeRef, "integer range produces values", diagnostic);
    tests.expect(rangeRefRejected,
        "reference foreach rejects storage-free integer ranges (" + diagnostic + ")");

    const std::filesystem::path edition2 = writeProject(
        root / "edition2", "ForeachEdition2", R"NS(
class Program
{
    public static int Main()
    {
        int[] values = new int[1];
        foreach (ref mut int value in values) { value = 42; }
        return values[0];
    }
}
)NS", 2);
    diagnostic.clear();
    const bool edition2Rejected =
        rejectedWith(edition2, "requires <Language>3</Language>", diagnostic);
    tests.expect(edition2Rejected,
        "reference iteration is edition-gated (" + diagnostic + ")");

    const std::filesystem::path trait = writeProject(
        root / "trait", "ForeachTrait", R"NS(
namespace ForeachTrait
{
    public trait IIterable { owned Enumerator GetEnumerator(); }

    public class Sequence
    {
        public int start;
        public Sequence(int value) { start = value; }
    }

    public class Enumerator
    {
        private int current;
        private bool pending;
        public Enumerator(int value) { current = value; pending = true; }
        ~Enumerator() { Program.Disposals = Program.Disposals + 1; }
        public bool MoveNext()
        {
            if (!pending) return false;
            pending = false;
            return true;
        }
        public int Current { get { return current; } }
    }

    impl IIterable for Sequence
    {
        public owned Enumerator GetEnumerator() { return new Enumerator(start); }
    }

    public class Program
    {
        public static int Disposals = 0;
        public static int Main()
        {
            Sequence sequence = new Sequence(40);
            int result = 0;
            foreach (int value in sequence)
            {
                result = value;
                break;
            }
            return result + Disposals + Disposals;
        }
    }
}
)NS");
    diagnostic.clear();
    const bool traitExecuted = compileAndRun(trait, 42, diagnostic);
    tests.expect(traitExecuted,
        "trait-provided iterator witnesses resolve statically and clean up on break (" +
            diagnostic + ")");

    const std::filesystem::path nonReferenceCurrent = writeProject(
        root / "non-ref-current", "ForeachNonRefCurrent", R"NS(
namespace ForeachNonRefCurrent
{
    public class Enumerator
    {
        public bool MoveNext() { return false; }
        public int Current { get { return 0; } }
    }
    public class Sequence
    {
        public Enumerator GetEnumerator() { return new Enumerator(); }
    }
    public class Program
    {
        public static int Main()
        {
            Sequence sequence = new Sequence();
            foreach (ref mut int value in sequence) { value = 1; }
            return 0;
        }
    }
}
)NS");
    diagnostic.clear();
    tests.expect(rejectedWith(
            nonReferenceCurrent, "reference-local initializer must be a mutable addressable", diagnostic),
        "ref mut iteration requires Current/indexer to expose mutable storage (" + diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
