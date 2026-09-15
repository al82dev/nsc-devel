#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

nativesharp::ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    nativesharp::driver::Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const nativesharp::ExitCode code = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return code;
}

void writeProject(const std::filesystem::path& root, std::string_view source)
{
    writeFile(root / "ReferenceLifetime.nsproj",
        "<Project><PropertyGroup><Name>ReferenceLifetimeItem316</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Main.ns", source);
}

void expectReject(
    nativesharp::tests::TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    std::string_view diagnostic)
{
    const auto caseRoot = root / std::string(name);
    writeProject(caseRoot, source);
    const std::string project = (caseRoot / "ReferenceLifetime.nsproj").string();
    const std::array<std::string_view, 3> arguments = {
        "--dump-project-oop-hir", project, "-O0"};
    std::string output;
    std::string error;
    tests.expect(runDriver(arguments, output, error) != nativesharp::ExitCode::Success,
        std::string(name) + " lifetime violation is rejected");
    tests.expect(error.find(diagnostic) != std::string::npos,
        std::string(name) + " reports the reference lifetime boundary (" + error + ")");
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item316_reference_lifetime_analysis_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    const auto validRoot = root / "valid";
    writeProject(validRoot, R"NS(
public class Program
{
    private static ref int ReturnNestedCaller(ref int caller)
    {
        {
            ref int nested = ref caller;
            return ref nested;
        }
    }

    private static void ExerciseScopes(ref int caller)
    {
        ref int methodAlias = ref caller;
        {
            int local = 4;
            ref int localAlias = ref local;
            ref int inherited = ref methodAlias;
            localAlias += 1;
            inherited += localAlias;
        }
    }

    public static int Main()
    {
        int value = 10;
        ExerciseScopes(ref value);
        ReturnNestedCaller(ref value) += 2;
        return value == 17 ? 0 : 1;
    }
}
)NS");

    const std::string project = (validRoot / "ReferenceLifetime.nsproj").string();
    std::string output;
    std::string error;
    const std::array<std::string_view, 3> bodyArguments = {
        "--dump-project-method-bodies", project, "-O0"};
    const bool bodies = runDriver(bodyArguments, output, error) == ExitCode::Success;
    tests.expect(bodies, "nested reference regions pass semantic analysis (" + error + ")");
    if (bodies)
    {
        tests.expect(output.find("name=methodAlias") != std::string::npos &&
                output.find("escape-origin=external") != std::string::npos,
            "caller storage retains an external origin");
        tests.expect(output.find("name=localAlias") != std::string::npos &&
                output.find("escape-origin=frame-local") != std::string::npos,
            "local storage retains a frame-local origin");
        tests.expect(output.find("max-scope=") != std::string::npos &&
                output.find("alias-scope=") != std::string::npos,
            "the method-body dump exposes maximum and actual lifetime regions");
    }

    const std::string executable = (validRoot / "ReferenceLifetime.exe").string();
    const std::array<std::string_view, 5> executableArguments = {
        "-O0", "--emit-project-oop-exe", project, "-o", executable};
    output.clear();
    error.clear();
    const bool emitted = runDriver(executableArguments, output, error) == ExitCode::Success;
    tests.expect(emitted, "reference-region fixture links at O0 (" + error + ")");
    if (emitted)
    {
        const std::string command = "\"" + executable + "\"";
        tests.expect(std::system(command.c_str()) == 0,
            "nested aliases remain valid only while their source regions are live");
    }

    expectReject(tests, root, "nested_local_return", R"NS(
public class Program
{
    private static ref int Bad()
    {
        int local = 1;
        {
            ref int alias = ref local;
            return ref alias;
        }
    }
    public static int Main() { return 0; }
}
)NS", "cannot expose storage owned by the current stack frame");

    expectReject(tests, root, "struct_ref_indexer", R"NS(
public struct Cell
{
    public int Value;
    public ref int this[int index] { get { return ref Value; } }
}
public class Program
{
    private static ref int Bad()
    {
        Cell cell;
        return ref cell[0];
    }
    public static int Main() { return 0; }
}
)NS", "cannot expose storage owned by the current stack frame");

    expectReject(tests, root, "vla_reference", R"NS(
public class Program
{
    private static ref int Bad(int count)
    {
        int[count] values;
        ref int alias = ref values[0];
        return ref alias;
    }
    public static int Main() { return 0; }
}
)NS", "cannot expose storage owned by the current stack frame");

    return tests.resultKeeping(root);
}
