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
    return result != ExitCode::Success;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t01_range_index";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    const std::filesystem::path positive = root / "positive";
    writeProject(positive, "RangeIndex", R"NS(
namespace RangeIndex
{
    public borrowed struct View<T>
    {
        private T* data;
        private int count;

        public View(T* value, int length) { data = value; count = length; }
        public int Length() { Protocol.lengthCalls = Protocol.lengthCalls + 1; return count; }
        public View<T> Slice(int start, int length)
        {
            Protocol.sliceCalls = Protocol.sliceCalls + 1;
            return new View<T>(data + start, length);
        }
        public T this[int index] { get { return data[index]; } }
    }

    public class Protocol
    {
        public static int receiverCalls = 0;
        public static int lengthCalls = 0;
        public static int sliceCalls = 0;

        private static View<int> Source(int[] values)
        {
            receiverCalls = receiverCalls + 1;
            return new View<int>(&values[0], values.Length);
        }

        public static int Main()
        {
            int[] values = new int[6];
            values[0] = 10;
            values[1] = 20;
            values[2] = 40;
            values[3] = 2;
            values[4] = 7;
            values[5] = 9;

            View<int> middle = Source(values)[1..^2];
            if (receiverCalls != 1 || lengthCalls != 1 || sliceCalls != 1 ||
                middle.Length() != 3 || middle[0] + middle[1] + middle[2] != 62)
                { return 1; }

            View<int> all = middle[..];
            View<int> tail = middle[1..];
            View<int> prefix = middle[..^1];
            View<int> empty = middle[^0..^0];
            if (all.Length() != 3 || tail.Length() != 2 || prefix.Length() != 2 ||
                empty.Length() != 0) { return 2; }

            if (values[^1] != 9) { return 3; }
            int[,] matrix = new int[2, 2];
            matrix[1, 0] = 42;
            if (matrix[^1, 0] != 42) { return 4; }
            delete[] matrix;
            delete[] values;
            return 42;
        }
    }
}
)NS");

    const std::string positiveProject = (positive / "RangeIndex.nsproj").string();
    const std::string executable = (positive / "RangeIndex.exe").string();
    const std::array<std::string_view, 5> emitArguments = {
        "--emit-project-oop-exe", positiveProject, "-O0", "-o", executable};
    std::string output;
    std::string error;
    const ExitCode emitted = runDriver(emitArguments, output, error);
    tests.expect(
        emitted == ExitCode::Success,
        "O0 compiles half-open/open/from-end ranges through the static Length/Slice protocol (" +
            error + output + ")");
    if (emitted == ExitCode::Success)
    {
        tests.expect(
            std::system(("\"" + executable + "\"").c_str()) == 42,
            "range slicing, array ^n and mixed rectangular ^n execute with one receiver evaluation");
    }

    const std::array<std::string_view, 3> hirArguments = {
        "--dump-project-oop-hir", positiveProject, "-O0"};
    output.clear();
    error.clear();
    tests.expect(
        runDriver(hirArguments, output, error) == ExitCode::Success &&
            output.find("range-slice") != std::string::npos,
        "HIR exposes range-slice as one compound protocol operation (" + error + output + ")");

    const std::array<std::string_view, 3> llvmArguments = {
        "--dump-project-oop-llvm", positiveProject, "-O0"};
    output.clear();
    error.clear();
    tests.expect(
        runDriver(llvmArguments, output, error) == ExitCode::Success &&
            output.find("range.length") != std::string::npos &&
            output.find("range.count") != std::string::npos &&
            output.find("range.slice") != std::string::npos &&
            output.find("range.index.fromend") != std::string::npos,
        "LLVM O0 publishes one Length result, half-open count, bounds guard and per-dimension ^n (" +
            error + ")");

    const std::filesystem::path proven = root / "proven";
    writeProject(proven, "ProvenRange", R"NS(
struct View
{
    public int Length() { return 4; }
    public View Slice(int start, int length) { return this; }
}
class Program
{
    public static int Main()
    {
        View value = new View();
        View all = value[..];
        return all.Length();
    }
}
)NS");
    const std::string provenProject = (proven / "ProvenRange.nsproj").string();
    const std::array<std::string_view, 3> provenLlvmArguments = {
        "--dump-project-oop-llvm", provenProject, "-O0"};
    output.clear();
    error.clear();
    tests.expect(
        runDriver(provenLlvmArguments, output, error) == ExitCode::Success &&
            output.find("range.length") != std::string::npos &&
            output.find("range.slice.trap") == std::string::npos,
        "compile-time endpoint proofs remove the range guard before Release optimization (" +
            error + ")");

    const std::filesystem::path fromEndZero = root / "from-end-zero";
    writeProject(fromEndZero, "FromEndZero", R"NS(
class Program
{
    public static int Main()
    {
        int[] values = new int[1];
        return values[^0];
    }
}
)NS");
    const std::string fromEndZeroProject = (fromEndZero / "FromEndZero.nsproj").string();
    const std::string fromEndZeroExecutable = (fromEndZero / "FromEndZero.exe").string();
    const std::array<std::string_view, 5> fromEndZeroArguments = {
        "--emit-project-oop-exe", fromEndZeroProject, "-O0", "-o", fromEndZeroExecutable};
    output.clear();
    error.clear();
    const bool fromEndZeroEmitted =
        runDriver(fromEndZeroArguments, output, error) == ExitCode::Success;
    tests.expect(fromEndZeroEmitted, "^0 element fixture compiles (" + error + output + ")");
    if (fromEndZeroEmitted)
    {
        tests.expect(
            std::system(("\"" + fromEndZeroExecutable + "\"").c_str()) != 0,
            "^0 denotes the one-past endpoint and traps when used as an element index");
    }

    std::string diagnostic;
    const bool negativeRejected = rejected(root / "negative", R"NS(
struct View
{
    public int Length() { return 4; }
    public View Slice(int start, int length) { return this; }
}
class Program
{
    public static int Main()
    {
        View value = new View();
        View invalid = value[-1..2];
        return 0;
    }
}
)NS", diagnostic);
    tests.expect(
        negativeRejected && diagnostic.find("cannot be negative") != std::string::npos,
        "negative constant endpoints are rejected before lowering (" + diagnostic + ")");

    diagnostic.clear();
    const bool protocolRejected = rejected(root / "protocol", R"NS(
struct View { public int Length() { return 4; } }
class Program
{
    public static int Main()
    {
        View value = new View();
        View invalid = value[1..2];
        return 0;
    }
}
)NS", diagnostic);
    tests.expect(
        protocolRejected && diagnostic.find("Length()") != std::string::npos &&
            diagnostic.find("Slice") != std::string::npos,
        "lookalike receivers missing either protocol member are rejected (" + diagnostic + ")");

    diagnostic.clear();
    const bool arraySliceRejected = rejected(root / "array-slice", R"NS(
class Program
{
    public static int Main()
    {
        int[] values = new int[4];
        int[] invalid = values[1..3];
        return 0;
    }
}
)NS", diagnostic);
    tests.expect(
        arraySliceRejected && diagnostic.find("cannot implicitly allocate") != std::string::npos,
        "heap-array slicing remains explicit and cannot hide an allocation (" + diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
