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
        "</Name><Language>3</Language><OutputType>Exe</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
        "</ItemGroup></Project>");
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

bool dump(
    std::string_view command,
    const std::filesystem::path& project,
    std::string& output,
    std::string& error)
{
    const std::string projectText = project.string();
    const std::array<std::string_view, 3> arguments = {command, projectText, "-O0"};
    return runDriver(arguments, output, error) == ExitCode::Success;
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
    if (result != ExitCode::Success) return false;
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
    std::string output;
    std::string error;
    const bool accepted = dump("--dump-project-method-bodies", project, output, error);
    diagnostic = error + output;
    return !accepted && diagnostic.find(expected) != std::string::npos;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3-t08-f06-directed";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) /
            "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    const std::filesystem::path valid = writeProject(
        root / "valid", "TailCalls", R"NS(
namespace TailCalls
{
    public class Program
    {
        public static noinline int Count(int remaining, int value)
        {
            if (remaining == 0) return value;
            must_tail return Count(remaining - 1, value + 1);
        }

        public static noinline T GenericCount<T>(int remaining, T value)
        {
            if (remaining == 0) return value;
            must_tail return GenericCount<T>(remaining - 1, value);
        }

        public static inline int Hint(int value) { return value; }

        public static int Main()
        {
            return Count(21, 21) + GenericCount<int>(1, 0) + Hint(0);
        }
    }
}
)NS");

    std::string output;
    std::string error;
    tests.expect(dump("--dump-project-oop-mir", valid, output, error) &&
            output.find("must-tail=true") != std::string::npos,
        "O0 MIR publishes verified must-tail call sites (" + error + ")");

    output.clear();
    error.clear();
    tests.expect(dump("--dump-project-oop-llvm", valid, output, error) &&
            output.find("musttail call") != std::string::npos &&
            output.find("noinline") != std::string::npos &&
            output.find("inlinehint") != std::string::npos,
        "LLVM preserves musttail and independent inline/noinline hints (" + error + ")");

    std::string executionDiagnostic;
    tests.expect(compileAndRun(valid, 42, executionDiagnostic),
        "O0/NSLink executes recursive and generic must_tail calls (" +
            executionDiagnostic + ")");

    const std::filesystem::path cleanup = writeProject(
        root / "cleanup", "TailCleanup", R"NS(
namespace TailCleanup
{
    public class Owner { ~Owner() {} }
    public class Program
    {
        public static int Target(int value) { return value; }
        public static int Invalid(int value)
        {
            raii Owner owner = new Owner();
            must_tail return Target(value);
        }
        public static int Main() { return 0; }
    }
}
)NS");
    std::string cleanupDiagnostic;
    tests.expect(rejectedWith(cleanup, "must_tail call has pending cleanup", cleanupDiagnostic),
        "pending destructors reject required tail calls (" + cleanupDiagnostic + ")");

    const std::filesystem::path abi = writeProject(
        root / "abi", "TailAbi", R"NS(
namespace TailAbi
{
    public class Program
    {
        public static int Target(int value) { return value; }
        public static int Invalid() { must_tail return Target(42); }
        public static int Main() { return 0; }
    }
}
)NS");
    std::string abiDiagnostic;
    tests.expect(rejectedWith(abi, "must_tail caller and callee ABIs are incompatible", abiDiagnostic),
        "different Win64 source signatures reject must_tail (" + abiDiagnostic + ")");

    const std::filesystem::path nonCall = writeProject(
        root / "non-call", "TailNonCall", R"NS(
namespace TailNonCall
{
    public class Program
    {
        public static int Invalid(int value) { must_tail return value; }
        public static int Main() { return 0; }
    }
}
)NS");
    std::string nonCallDiagnostic;
    tests.expect(rejectedWith(nonCall, "must_tail return requires a direct call expression", nonCallDiagnostic),
        "must_tail cannot silently degrade into an ordinary return (" + nonCallDiagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
