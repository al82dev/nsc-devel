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

std::string functionContaining(std::string_view module, std::string_view marker)
{
    const std::size_t markerAt = module.find(marker);
    if (markerAt == std::string_view::npos) return {};
    const std::size_t begin = module.rfind("define ", markerAt);
    const std::size_t end = module.find("\n}\n", markerAt);
    if (begin == std::string_view::npos || end == std::string_view::npos) return {};
    return std::string(module.substr(begin, end + 3U - begin));
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t01_typed_parameter_packs";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    const std::filesystem::path positive = root / "positive";
    writeProject(positive, "TypedPacks", R"NS(
namespace TypedPacks
{
    public copy borrowed struct ReadOnlySpan<T>
    {
        public T* start;
        public int count;
    }

    public class Program
    {
        private static int trace = 0;

        private static noheap int Mark(int value)
        {
            trace = trace * 10 + value;
            return value;
        }

        private static noheap int Sum(params ReadOnlySpan<int> values)
        {
            int total = 0;
            int index = 0;
            while (index < values.count)
            {
                total = total + values.start[index];
                index = index + 1;
            }
            return total;
        }

        private static noheap int Forward(ReadOnlySpan<int> values)
        {
            return Sum(values);
        }

        private static noheap int Pick(int value) { return 42; }
        private static noheap int Pick(params ReadOnlySpan<int> values) { return 0; }

        public static noheap int Main()
        {
            if (Sum() != 0) { return 1; }
            if (Sum(42) != 42) { return 2; }
            trace = 0;
            if (Sum(Mark(1), Mark(2), Mark(3)) != 6 || trace != 123) { return 3; }
            if (Pick(7) != 42) { return 4; }

            int* raw = stackalloc int[2];
            raw[0] = 20;
            raw[1] = 22;
            ReadOnlySpan<int> ready = { start = raw, count = 2 };
            if (Forward(ready) != 42) { return 5; }
            return 42;
        }
    }
}
)NS");

    const std::string positiveProject = (positive / "TypedPacks.nsproj").string();
    const std::string executable = (positive / "TypedPacks.exe").string();
    const std::array<std::string_view, 5> emitArguments = {
        "--emit-project-oop-exe", positiveProject, "-O0", "-o", executable};
    std::string output;
    std::string error;
    const ExitCode emitted = runDriver(emitArguments, output, error);
    tests.expect(
        emitted == ExitCode::Success,
        "O0 accepts zero/one/many, direct-span forwarding, noheap, and fixed-over-pack overloads (" +
            error + output + ")");
    if (emitted == ExitCode::Success)
    {
        const std::string command = "\"" + executable + "\"";
        tests.expect(
            std::system(command.c_str()) == 42,
            "typed parameter packs execute with left-to-right element evaluation");
    }

    const std::array<std::string_view, 3> llvmArguments = {
        "--dump-project-oop-llvm", positiveProject, "-O0"};
    output.clear();
    error.clear();
    const bool dumped = runDriver(llvmArguments, output, error) == ExitCode::Success;
    const std::string packFunction = functionContaining(output, "params.elements");
    tests.expect(
        dumped && !packFunction.empty() &&
            packFunction.find("alloca [3 x i32]") != std::string::npos &&
            packFunction.find("params.span") != std::string::npos &&
            packFunction.find("call ptr @__nativesharp_alloc") == std::string::npos,
        "expanded packs use explicit stack storage and make no heap-allocation call (" + error + ")");

    std::string diagnostic;
    const bool ownerRejected = rejected(root / "owner", R"NS(
borrowed struct ReadOnlySpan<T> { public T* start; public int count; }
class Resource { }
class Program
{
    private static int Consume(params ReadOnlySpan<Resource> values) { return values.count; }
    public static int Main()
    {
        owned Resource value = new Resource();
        return Consume(move(value));
    }
}
)NS", diagnostic);
    tests.expect(
        ownerRejected && diagnostic.find("no viable overload") != std::string::npos,
        "expanded packs reject ownership transfer and non-copyable elements (" + diagnostic + ")");

    diagnostic.clear();
    const bool directEscapeRejected = rejected(root / "direct-escape", R"NS(
borrowed struct ReadOnlySpan<T> { public T* start; public int count; }
class Program
{
    private static ReadOnlySpan<int> Capture(params ReadOnlySpan<int> values) { return values; }
    public static ReadOnlySpan<int> Leak() { return Capture(20, 22); }
    public static int Main() { return 0; }
}
)NS", diagnostic);
    tests.expect(
        directEscapeRejected &&
            diagnostic.find("expanded parameter pack") != std::string::npos &&
            diagnostic.find("cannot escape") != std::string::npos,
        "a view backed by an expanded pack cannot escape through a direct return (" + diagnostic + ")");

    diagnostic.clear();
    const bool localEscapeRejected = rejected(root / "local-escape", R"NS(
borrowed struct ReadOnlySpan<T> { public T* start; public int count; }
class Program
{
    private static ReadOnlySpan<int> Capture(params ReadOnlySpan<int> values) { return values; }
    public static ReadOnlySpan<int> Leak()
    {
        ReadOnlySpan<int> local = Capture(20, 22);
        return local;
    }
    public static int Main() { return 0; }
}
)NS", diagnostic);
    tests.expect(
        localEscapeRejected && diagnostic.find("cannot be returned") != std::string::npos,
        "a local view retains the expanded-pack frame origin (" + diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
