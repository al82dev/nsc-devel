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
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t01_static_interpolation";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    const std::filesystem::path positive = root / "positive";
    writeProject(positive, "StaticInterpolation", R"NS(
namespace StaticInterpolation
{
    public class Program
    {
        private static noheap int StaticLength()
        {
            string value = $"left={{ {-42:D5} }} hex={31:X4} zero={-0:D2} bool={true}";
            return value.Length;
        }

        public static int Main()
        {
            string value = $"left={{ {-42:D5} }} hex={31:X4} zero={-0:D2} bool={true}";
            if (value != "left={ -0042 } hex=001F zero=00 bool=true") { return 1; }
            if (StaticLength() != 41) { return 2; }
            return 42;
        }
    }
}
)NS");

    const std::string project = (positive / "StaticInterpolation.nsproj").string();
    const std::string executable = (positive / "StaticInterpolation.exe").string();
    const std::array<std::string_view, 5> emitArguments = {
        "--emit-project-oop-exe", project, "-O0", "-o", executable};
    std::string output;
    std::string error;
    const ExitCode emitted = runDriver(emitArguments, output, error);
    tests.expect(
        emitted == ExitCode::Success,
        "O0 folds formats, escaped braces, bools, signed width and canonical zero (" +
            error + output + ")");
    if (emitted == ExitCode::Success)
    {
        tests.expect(
            std::system(("\"" + executable + "\"").c_str()) == 42,
            "folded interpolation executes as the expected immutable literal");
    }

    const std::array<std::string_view, 3> hirArguments = {
        "--dump-project-oop-hir", project, "-O0"};
    output.clear();
    error.clear();
    tests.expect(
        runDriver(hirArguments, output, error) == ExitCode::Success,
        "HIR accepts the compile-time handler result in a noheap callable (" +
            error + output + ")");

    const std::array<std::string_view, 3> llvmArguments = {
        "--dump-project-oop-llvm", project, "-O0"};
    output.clear();
    error.clear();
    tests.expect(
        runDriver(llvmArguments, output, error) == ExitCode::Success &&
            output.find("String.ToString") == std::string::npos &&
            output.find("String.Concat") == std::string::npos,
        "LLVM O0 uses immutable data without String formatting or concatenation calls (" +
            error + ")");

    const std::filesystem::path missingHandler = root / "missing-handler";
    writeProject(missingHandler, "MissingHandler", R"NS(
class Program
{
    public static int Main()
    {
        int value = 42;
        string text = $"value={value}";
        return text.Length;
    }
}
)NS");
    const std::string missingProject = (missingHandler / "MissingHandler.nsproj").string();
    const std::array<std::string_view, 3> missingArguments = {
        "--check-project-oop-hir", missingProject, "-O0"};
    output.clear();
    error.clear();
    tests.expect(
        runDriver(missingArguments, output, error) != ExitCode::Success &&
            (error + output).find("unknown identifier 'String'") != std::string::npos,
        "a runtime hole without an explicit formatting surface fails closed (" +
            error + output + ")");

    const std::filesystem::path malformed = root / "malformed";
    writeProject(malformed, "MalformedInterpolation", R"NS(
class Program
{
    public static int Main()
    {
        string text = $"value={42";
        return text.Length;
    }
}
)NS");
    const std::string malformedProject = (malformed / "MalformedInterpolation.nsproj").string();
    const std::array<std::string_view, 3> malformedArguments = {
        "--check-project-oop-hir", malformedProject, "-O0"};
    output.clear();
    error.clear();
    tests.expect(
        runDriver(malformedArguments, output, error) != ExitCode::Success &&
            (error + output).find("invalid or unterminated interpolated string literal") !=
                std::string::npos,
        "malformed interpolation receives a stable causal diagnostic (" + error + output + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
