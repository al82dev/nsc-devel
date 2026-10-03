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
    std::string_view outputType,
    std::string_view source,
    std::string_view items = "<Compile Include=\"**/*.ns\" />")
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language><OutputType>" + std::string(outputType) +
        "</OutputType></PropertyGroup><ItemGroup>" + std::string(items) +
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
    std::string& diagnostic,
    std::string_view items = "<Compile Include=\"**/*.ns\" />")
{
    const auto project = writeProject(directory, name, "Exe", source, items);
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
    const auto project = writeProject(directory, name, "Exe", source);
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
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t04_controlled_specialization";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    std::string diagnostic;
    tests.expect(compileAndRun(root / "productive", "ControlledSpecialization", R"NS(
class Selector
{
    public specialize static int Choose<T>(T value) where T : unmanaged { return 42; }
    public static int Choose<T>(T value) { return 1; }
}
class Program
{
    public static int Main()
    {
        if (Selector.Choose<string>("fallback") != 1) { return 2; }
        return Selector.Choose(0);
    }
}
)NS", 42, diagnostic),
        "a strictly stronger opt-in implementation wins independent of declaration order while "
        "an unsatisfied specialization falls back in O0/NSLink (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "diamond", "DiamondOverlap", R"NS(
class Selector
{
    public static int Choose<T>(T value) { return 1; }
    public specialize static int Choose<T>(T value) where T : unmanaged { return 2; }
    public specialize static int Choose<T>(T value) where T : copy { return 3; }
}
class Program { public static int Main() { return 0; } }
)NS", "overlap with incomparable constraints", diagnostic),
        "diamond overlaps are rejected before use (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "equivalent", "EquivalentConstraints", R"NS(
class Selector
{
    public static int Choose<T>(T value) { return 1; }
    public specialize static int Choose<T>(T value) where T : unmanaged { return 2; }
    public specialize static int Choose<T>(T value) where T : unmanaged { return 3; }
}
class Program { public static int Main() { return 0; } }
)NS", "constraints equivalent", diagnostic),
        "equivalent constraints cannot depend on declaration order (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "opt_in", "MissingOptIn", R"NS(
class Selector
{
    public static int Choose<T>(T value) { return 1; }
    public static int Choose<T>(T value) where T : unmanaged { return 2; }
}
class Program { public static int Main() { return 0; } }
)NS", "must opt in with the specialize modifier", diagnostic),
        "a stronger implementation is rejected without explicit opt-in (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "fallback", "MissingFallback", R"NS(
class Selector
{
    public specialize static int Choose<T>(T value) where T : unmanaged { return 2; }
}
class Program { public static int Main() { return 0; } }
)NS", "has no less-constrained fallback implementation", diagnostic),
        "a controlled specialization cannot exist without a fallback (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "modifier", "InvalidModifierTarget", R"NS(
class Bad { specialize int value; }
class Program { public static int Main() { return 0; } }
)NS", "specialize applies only to an ordinary generic method", diagnostic),
        "the contextual modifier is rejected on non-method declarations (" + diagnostic + ")");

    const std::filesystem::path producer = root / "modules" / "Producer";
    const auto producerProject = writeProject(producer, "SpecializationApi", "StaticLibrary", R"NS(
namespace SpecializationApi
{
    public class Selector
    {
        public static int Choose<T>(T value) { return 1; }
        public specialize static int Choose<T>(T value) where T : unmanaged { return 42; }
        public static int Probe() { return Choose<int>(0); }
    }
}
)NS");
    const std::string producerProjectText = producerProject.string();
    const std::string libraryText = (producer / "SpecializationApi.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "--emit-project-oop-lib", producerProjectText, "-O0", "-o", libraryText};
    std::string output;
    std::string error;
    const ExitCode producerResult = runDriver(producerArguments, output, error);
    diagnostic = error + output;
    const std::string_view consumerItems =
        "<Compile Include=\"**/*.ns\" />"
        "<LibraryReference Include=\"../Producer/SpecializationApi.lib\" />";
    const bool moduleRoundTrip = producerResult == ExitCode::Success && compileAndRun(
        root / "modules" / "Consumer", "SpecializationConsumer", R"NS(
using SpecializationApi;
class Program
{
    public static int Main() { return Selector.Choose<int>(0); }
}
)NS", 42, diagnostic, consumerItems);
    tests.expect(moduleRoundTrip,
        "transported public templates preserve the same specialization order in a separate "
        "consumer build (" + diagnostic + ")");

    if (producerResult != ExitCode::Success)
        std::cerr << "producer failed: " << diagnostic << '\n';
    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
