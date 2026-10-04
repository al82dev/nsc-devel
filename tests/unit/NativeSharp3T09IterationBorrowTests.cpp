#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Language/IterationSemantics.h"

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
    writeFile(project,
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language><OutputType>Exe</OutputType></PropertyGroup>"
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

bool compileAndRun(const std::filesystem::path& project, int expected, std::string& diagnostic)
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
        diagnostic += "driver exit=" + std::to_string(static_cast<int>(result));
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
    return !accepted && diagnostic.find(expected) != std::string::npos;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3-t09-f04-directed";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) /
            "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    const std::filesystem::path mutableElements = writeProject(
        root / "mutable-elements", "IterationMutableElements", R"NS(
public class Program
{
    public static int Main()
    {
        int[] values = new int[2];
        values[0] = 10;
        values[1] = 20;
        foreach (ref mut int value in values)
        {
            value = value + 5;
            values[1] = values[1] + 1;
        }
        values = new int[1];
        values[0] = 42;
        return values[0];
    }
}
)NS");
    std::string diagnostic;
    const bool mutableElementsAccepted = compileAndRun(mutableElements, 42, diagnostic);
    tests.expect(mutableElementsAccepted,
        "ref mut permits compatible element writes and releases the source loan after the loop (" +
            diagnostic + ")");

    const std::filesystem::path readonlyWrite = writeProject(
        root / "readonly-write", "IterationReadonlyWrite", R"NS(
public class Program
{
    public static int Main()
    {
        int[] values = new int[1];
        foreach (ref int value in values) { values[0] = value; }
        return 0;
    }
}
)NS");
    diagnostic.clear();
    const bool readonlyWriteRejected = rejectedWith(
        readonlyWrite, "cannot mutate foreach source storage", diagnostic);
    tests.expect(readonlyWriteRejected,
        "a shared iteration loan rejects direct source element mutation (" + diagnostic + ")");

    const std::filesystem::path replaceSource = writeProject(
        root / "replace-source", "IterationReplaceSource", R"NS(
public class Program
{
    public static int Main()
    {
        int[] values = new int[1];
        foreach (ref mut int value in values) { values = new int[2]; }
        return 0;
    }
}
)NS");
    diagnostic.clear();
    const bool replacementRejected = rejectedWith(
        replaceSource, "cannot mutate foreach source storage", diagnostic);
    tests.expect(replacementRejected,
        "an exclusive element loan still rejects replacement of source storage (" + diagnostic + ")");

    const std::filesystem::path valueSnapshot = writeProject(
        root / "value-snapshot", "IterationValueSnapshot", R"NS(
public class Program
{
    public static int Main()
    {
        int[] values = new int[1];
        values[0] = 40;
        int total = 0;
        foreach (int value in values)
        {
            total = total + value;
            values = new int[1];
            values[0] = 2;
        }
        return total + values[0];
    }
}
)NS");
    diagnostic.clear();
    const bool valueSnapshotAccepted = compileAndRun(valueSnapshot, 42, diagnostic);
    tests.expect(valueSnapshotAccepted,
        "value iteration keeps its evaluated-once snapshot without inventing a lending loan (" +
            diagnostic + ")");

    constexpr std::string_view spanPrelude = R"NS(
namespace NativeSharp.Core
{
    public borrowed struct Span<T>
    {
        private T* start;
        private int count;
        public Span(T* data, int length) { start = data; count = length; }
        public int Length() { return count; }
        public T* Data() { return start; }
        public readonly int Count() { return count; }
        public void Reset() { count = 0; }
    }
}
)NS";

    const std::filesystem::path readonlyCall = writeProject(
        root / "readonly-call", "IterationReadonlyCall", std::string(spanPrelude) + R"NS(
using NativeSharp.Core;
public class Program
{
    public static int Main()
    {
        int* raw = stackalloc int[1];
        raw[0] = 40;
        Span<int> values = new Span<int>(raw, 1);
        foreach (ref mut int value in values)
        {
            value = value + values.Count() + 1;
        }
        return raw[0];
    }
}
)NS");
    diagnostic.clear();
    const bool readonlyCallAccepted = compileAndRun(readonlyCall, 42, diagnostic);
    tests.expect(readonlyCallAccepted,
        "readonly source members remain callable during a lending iteration (" + diagnostic + ")");

    const std::filesystem::path mutatingCall = writeProject(
        root / "mutating-call", "IterationMutatingCall", std::string(spanPrelude) + R"NS(
using NativeSharp.Core;
public class Program
{
    public static int Main()
    {
        int* raw = stackalloc int[1];
        Span<int> values = new Span<int>(raw, 1);
        foreach (ref mut int value in values) { values.Reset(); }
        return 0;
    }
}
)NS");
    diagnostic.clear();
    const bool mutatingCallRejected = rejectedWith(
        mutatingCall, "could invalidate yielded references", diagnostic);
    tests.expect(mutatingCallRejected,
        "a mutating source member is rejected while the iterator lends element storage (" +
            diagnostic + ")");

    using namespace nativesharp::language;
    const IteratorProtocol lending{
        "builtin:int", IterationMode::MutableReference, true, true, true, true};
    tests.expect(mutationAllowedDuringBorrowedIteration(lending, SourceMutation::Element) &&
            !mutationAllowedDuringBorrowedIteration(lending, SourceMutation::Structural),
        "the language contract classifies element and structural mutation consistently");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
