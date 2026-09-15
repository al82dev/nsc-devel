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
    writeFile(root / "EscapeAnalysis.nsproj",
        "<Project><PropertyGroup><Name>UnifiedEscapeItem315</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Main.ns", source);
}

void expectReject(
    nativesharp::tests::TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view body,
    std::string_view diagnostic)
{
    const auto caseRoot = root / std::string(name);
    writeProject(caseRoot, body);
    const std::string project = (caseRoot / "EscapeAnalysis.nsproj").string();
    const std::array<std::string_view, 3> arguments = {
        "--dump-project-oop-hir", project, "-O0"};
    std::string output;
    std::string error;
    tests.expect(runDriver(arguments, output, error) != nativesharp::ExitCode::Success,
        std::string(name) + " escape is rejected");
    tests.expect(error.find(diagnostic) != std::string::npos,
        std::string(name) + " identifies its frame-owned origin (" + error + ")");
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item315_unified_escape_analysis_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    const auto validRoot = root / "valid";
    writeProject(validRoot, R"NS(
public class Program
{
    private static int Global;

    private static ref int Forward(ref int value) { return ref value; }

    private static ref int FromParameter(ref int value)
    {
        ref int first = ref value;
        ref int second = ref first;
        return ref second;
    }

    private static ref int FromCall(ref int value)
    {
        return ref Forward(ref value);
    }

    private static ref int FromStatic()
    {
        ref int alias = ref Global;
        return ref alias;
    }

    private static ref int FromPointer(int* pointer)
    {
        return ref *pointer;
    }

    public static int Main()
    {
        int value = 1;
        FromParameter(ref value) = 2;
        FromCall(ref value) = 3;
        FromStatic() = 4;
        FromPointer(&value) = 5;
        return value == 5 && Global == 4 ? 0 : 1;
    }
}
)NS");

    const std::string project = (validRoot / "EscapeAnalysis.nsproj").string();
    std::string output;
    std::string error;
    const std::array<std::string_view, 3> bodyArguments = {
        "--dump-project-method-bodies", project, "-O0"};
    const bool bodies = runDriver(bodyArguments, output, error) == ExitCode::Success;
    tests.expect(bodies, "caller and external reference origins pass analysis (" + error + ")");
    if (bodies)
    {
        tests.expect(output.find("name=first type=builtin:int ref=true") != std::string::npos &&
                output.find("escape-origin=external") != std::string::npos,
            "the method-body model exposes the stable reference origin");
    }

    const std::string executable = (validRoot / "EscapeAnalysis.exe").string();
    const std::array<std::string_view, 5> executableArguments = {
        "-O0", "--emit-project-oop-exe", project, "-o", executable};
    output.clear();
    error.clear();
    const bool emitted = runDriver(executableArguments, output, error) == ExitCode::Success;
    tests.expect(emitted, "escape-safe forwarding links at O0 (" + error + ")");
    if (emitted)
    {
        const std::string command = "\"" + executable + "\"";
        tests.expect(std::system(command.c_str()) == 0,
            "safe aliases preserve caller, static, and pointer storage at runtime");
    }

    expectReject(tests, root, "local_alias", R"NS(
public class Program
{
    private static ref int Bad()
    {
        int local = 1;
        ref int alias = ref local;
        return ref alias;
    }
    public static int Main() { return 0; }
}
)NS", "cannot expose storage owned by the current stack frame");

    expectReject(tests, root, "chained_alias", R"NS(
public class Program
{
    private static ref int Bad()
    {
        int local = 1;
        ref int first = ref local;
        ref int second = ref first;
        return ref second;
    }
    public static int Main() { return 0; }
}
)NS", "cannot expose storage owned by the current stack frame");

    expectReject(tests, root, "forwarded_call", R"NS(
public class Program
{
    private static ref int Forward(ref int value) { return ref value; }
    private static ref int Bad()
    {
        int local = 1;
        return ref Forward(ref local);
    }
    public static int Main() { return 0; }
}
)NS", "cannot expose storage owned by the current stack frame");

    expectReject(tests, root, "value_parameter_alias", R"NS(
public class Program
{
    private static ref int Bad(int value)
    {
        ref int alias = ref value;
        return ref alias;
    }
    public static int Main() { return 0; }
}
)NS", "cannot expose a by-value or readonly parameter");

    expectReject(tests, root, "struct_field", R"NS(
public struct Pair { public int Value; }
public class Program
{
    private static ref int Bad()
    {
        Pair pair;
        pair.Value = 1;
        return ref pair.Value;
    }
    public static int Main() { return 0; }
}
)NS", "cannot expose storage owned by the current stack frame");

    expectReject(tests, root, "fixed_array_element", R"NS(
public class Program
{
    private static ref int Bad()
    {
        int[2] values;
        values[0] = 1;
        return ref values[0];
    }
    public static int Main() { return 0; }
}
)NS", "cannot expose storage owned by the current stack frame");

    return tests.resultKeeping(root);
}
