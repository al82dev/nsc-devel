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

void writeProject(const std::filesystem::path& root, std::string_view program)
{
    writeFile(root / "ReferenceLocals.nsproj",
        "<Project><PropertyGroup><Name>ReferenceLocalsItem312</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Main.ns", program);
}

void expectReject(
    nativesharp::tests::TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view program,
    std::string_view diagnostic)
{
    const auto caseRoot = root / std::string(name);
    writeProject(caseRoot, program);
    const std::string project = (caseRoot / "ReferenceLocals.nsproj").string();
    const std::array<std::string_view, 3> arguments = {
        "--dump-project-oop-hir", project, "-O0"};
    std::string output;
    std::string error;
    tests.expect(runDriver(arguments, output, error) != nativesharp::ExitCode::Success,
        std::string(name) + " is rejected");
    tests.expect(error.find(diagnostic) != std::string::npos,
        std::string(name) + " reports the reference-local contract (" + error + ")");
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item312_reference_locals_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    const auto validRoot = root / "valid";
    writeProject(validRoot, R"NS(
public struct Pair
{
    public int Left;
    public int Right;
}

public class Holder
{
    public int Value;
}

public class Program
{
    private static void SetThroughParameter(ref int target)
    {
        ref int alias = ref target;
        alias = 31;
    }

    public static int Main()
    {
        int first = 10;
        int untouched = 20;
        ref int alias = ref first;
        if (alias != 10) { return 1; }
        alias += 2;
        if (first != 12) { return 2; }

        ref int chained = ref alias;
        chained = 30;
        if (first != 30 || alias != 30) { return 3; }
        SetThroughParameter(ref chained);
        if (first != 31) { return 4; }

        Pair pair;
        pair.Left = 4;
        pair.Right = 5;
        ref int field = ref pair.Left;
        field++;
        if (pair.Left != 5 || pair.Right != 5) { return 5; }

        int* buffer = stackalloc int[2];
        buffer[0] = 7;
        buffer[1] = 8;
        ref int element = ref buffer[1];
        element += 2;
        if (buffer[0] != 7 || buffer[1] != 10) { return 6; }

        raii Holder holder = new Holder();
        holder.Value = 40;
        ref int member = ref holder.Value;
        member = 42;
        return holder.Value == 42 && untouched == 20 ? 0 : 7;
    }
}
)NS");

    const std::string project = (validRoot / "ReferenceLocals.nsproj").string();
    std::string output;
    std::string error;

    const std::array<std::string_view, 3> bodyArguments = {
        "--dump-project-method-bodies", project, "-O0"};
    const bool bodies = runDriver(bodyArguments, output, error) == ExitCode::Success;
    tests.expect(bodies, "reference locals pass semantic analysis (" + error + ")");
    if (bodies)
    {
        tests.expect(output.find("name=alias type=builtin:int ref=true") != std::string::npos &&
                output.find("name=chained type=builtin:int ref=true") != std::string::npos,
            "method model preserves reference-local identity and logical element type");
    }

    const std::array<std::string_view, 3> hirArguments = {
        "--dump-project-oop-hir", project, "-O0"};
    output.clear();
    error.clear();
    const bool hir = runDriver(hirArguments, output, error) == ExitCode::Success;
    tests.expect(hir, "reference locals lower to O0 HIR (" + error + ")");
    if (hir)
    {
        tests.expect(output.find("local-declaration type=builtin:int local=#") != std::string::npos &&
                output.find("reference-local=true") != std::string::npos,
            "HIR marks reference storage explicitly");
        tests.expect(output.find("pointer-load type=builtin:int") != std::string::npos,
            "HIR reads reference locals through their stored address");
    }

    const std::array<std::string_view, 3> mirArguments = {
        "--dump-project-oop-mir", project, "-O0"};
    output.clear();
    error.clear();
    const bool mir = runDriver(mirArguments, output, error) == ExitCode::Success;
    tests.expect(mir, "reference locals lower to O0 MIR (" + error + ")");
    if (mir)
    {
        tests.expect(output.find("reference-local=true") != std::string::npos &&
                output.find("pointer-load type=builtin:int") != std::string::npos,
            "MIR transports alias identity without a new pointer operation");
    }

    const std::array<std::string_view, 3> llvmArguments = {
        "--dump-project-oop-llvm", project, "-O0"};
    output.clear();
    error.clear();
    const bool llvm = runDriver(llvmArguments, output, error) == ExitCode::Success;
    tests.expect(llvm, "reference locals emit valid O0 LLVM (" + error + ")");
    if (llvm)
    {
        writeFile(validRoot / "ReferenceLocals.ll", output);
        tests.expect(output.find("alloca ptr") != std::string::npos &&
                output.find("local.load") != std::string::npos,
            "LLVM stores alias addresses in pointer-sized local slots");
    }

    const std::string executable = (validRoot / "ReferenceLocals.exe").string();
    const std::array<std::string_view, 5> executableArguments = {
        "-O0", "--emit-project-oop-exe", project, "-o", executable};
    output.clear();
    error.clear();
    const bool emitted = runDriver(executableArguments, output, error) == ExitCode::Success;
    tests.expect(emitted, "reference-local fixture links at O0 (" + error + ")");
    if (emitted)
    {
        const std::string command = "\"" + executable + "\"";
        tests.expect(std::system(command.c_str()) == 0,
            "aliases mutate locals, ref parameters, struct fields, raw elements, and object fields");
    }

    expectReject(tests, root, "literal", R"NS(
public class Program { public static int Main() { ref int value = ref 1; return 0; } }
)NS", "reference-local initializer must be a mutable addressable storage location");

    expectReject(tests, root, "readonly", R"NS(
public class Program { public static int Main() { const int source = 1; ref int value = ref source; return value; } }
)NS", "reference-local initializer must be a mutable addressable storage location");

    expectReject(tests, root, "mismatch", R"NS(
public class Program { public static int Main() { long source = 1; ref int value = ref source; return value; } }
)NS", "must exactly match declared type 'builtin:int'");

    expectReject(tests, root, "missing_ref", R"NS(
public class Program { public static int Main() { int source = 1; ref int value = source; return value; } }
)NS", "expected 'ref' before the reference-local initializer");

    expectReject(tests, root, "capture", R"NS(
public delegate int Getter();
public class Program
{
    public static int Main()
    {
        int source = 1;
        ref int value = ref source;
        Getter getter = () => value;
        return getter();
    }
}
)NS", "cannot be captured by a lambda because the delegate may outlive the referenced storage");

    expectReject(tests, root, "ref_var", R"NS(
public class Program { public static int Main() { int source = 1; ref var value = ref source; return value; } }
)NS", "reference locals require an explicit element type");

    return tests.resultKeeping(root);
}
