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

nativesharp::ExitCode runDriver(std::span<const std::string_view> arguments, std::string& output,
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
    writeFile(root / "Destroy.nsproj",
              "<Project><PropertyGroup><Name>DestroyItem319</Name></PropertyGroup>"
              "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Main.ns", source);
}

void expectReject(nativesharp::tests::TestContext& tests, const std::filesystem::path& root,
                  std::string_view name, std::string_view source, std::string_view diagnostic)
{
    const auto caseRoot = root / std::string(name);
    writeProject(caseRoot, source);
    const std::string project = (caseRoot / "Destroy.nsproj").string();
    const std::array<std::string_view, 3> arguments = {"--dump-project-oop-hir", project, "-O0"};
    std::string output;
    std::string error;
    tests.expect(runDriver(arguments, output, error) != nativesharp::ExitCode::Success,
                 std::string(name) + " is rejected");
    tests.expect(error.find(diagnostic) != std::string::npos,
                 std::string(name) + " reports the destruction contract (" + error + ")");
}
} // namespace

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root =
        std::filesystem::path(NSC_BINARY_DIR) / "item319_explicit_object_destruction_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    const auto validRoot = root / "valid";
    writeProject(validRoot, R"NS(
public class Log
{
    public static int Trace;
    public static void Step(int digit) { Trace = Trace * 10 + digit; }
}

public class Base
{
    ~Base() { Log.Step(1); }
}

public class Derived : Base
{
    public int Value;
    public Derived(int value) { Value = value; }
    ~Derived() { Log.Step(2); }
}

public struct Cell { public int Value; }

public class Program
{
    public static int Main()
    {
        long* words = stackalloc long[16];
        Derived* storage = (Derived*)words;
        Base first = new (storage) Derived(41);
        destroy first;
        if (Log.Trace != 21) { return 1; }

        Derived second = new (storage) Derived(42);
        if (second.Value != 42) { return 2; }
        destroy storage;
        if (Log.Trace != 2121) { return 3; }

        Cell* cells = stackalloc Cell[1];
        Cell* cell = new (cells) Cell();
        destroy cell;
        cell->Value = 7;
        return cell->Value == 7 ? 0 : 4;
    }
}
)NS");

    const std::string project = (validRoot / "Destroy.nsproj").string();
    std::string output;
    std::string error;

    const std::array<std::string_view, 3> hirArguments = {"--dump-project-oop-hir", project, "-O0"};
    const bool hir = runDriver(hirArguments, output, error) == ExitCode::Success;
    tests.expect(hir, "explicit destruction lowers to O0 HIR (" + error + ")");
    if (hir)
    {
        tests.expect(output.find("object-destruction type=builtin:void") != std::string::npos &&
                         output.find("destruction=dynamic-object-only") != std::string::npos,
                     "HIR distinguishes dynamic object-only destruction");
        tests.expect(output.find("destruction=trivial-object") != std::string::npos,
                     "HIR retains trivial struct lifetime termination without a free");
    }

    const std::array<std::string_view, 3> mirArguments = {"--dump-project-oop-mir", project, "-O0"};
    output.clear();
    error.clear();
    const bool mir = runDriver(mirArguments, output, error) == ExitCode::Success;
    tests.expect(mir, "explicit destruction lowers to O0 MIR (" + error + ")");
    if (mir)
    {
        tests.expect(output.find("object-destroy type=builtin:void") != std::string::npos &&
                         output.find("destruction=dynamic-object-only") != std::string::npos,
                     "MIR preserves the object/storage boundary");
        tests.expect(output.find("destroy-object=__nsc_destroy_object_") != std::string::npos,
                     "MIR dispatch tables expose a separate object-only entry");
    }

    const std::array<std::string_view, 3> llvmArguments = {"--dump-project-oop-llvm", project,
                                                           "-O0"};
    output.clear();
    error.clear();
    const bool llvm = runDriver(llvmArguments, output, error) == ExitCode::Success;
    tests.expect(llvm, "explicit destruction emits verifier-clean O0 LLVM (" + error + ")");
    if (llvm)
    {
        tests.expect(output.find("destroy.object.entry.addr") != std::string::npos &&
                         output.find("__nsc_destroy_object_") != std::string::npos,
                     "LLVM dispatches through the object-only descriptor entry");
        tests.expect(output.find("%nativesharp.class_descriptor = type { ptr, ptr, "
                                 "i64, i64, ptr, ptr, ptr, ptr }") != std::string::npos,
                     "the descriptor appends the object-only entry without moving "
                     "existing fields");
    }

    const std::string executable = (validRoot / "Destroy.exe").string();
    const std::array<std::string_view, 5> executableArguments = {"-O0", "--emit-project-oop-exe",
                                                                 project, "-o", executable};
    output.clear();
    error.clear();
    const bool emitted = runDriver(executableArguments, output, error) == ExitCode::Success;
    tests.expect(emitted, "explicit destruction fixture links at O0 (" + error + ")");
    if (emitted)
    {
        const std::string command = "\"" + executable + "\"";
        tests.expect(std::system(command.c_str()) == 0,
                     "dynamic derived/base destruction runs twice while caller "
                     "storage remains reusable");
    }

    expectReject(tests, root, "primitive", R"NS(
public class Program { public static int Main() { int value = 1; destroy value; return 0; } }
)NS",
                 "destroy requires a class/interface reference or a pointer to a "
                 "concrete class or struct");

    expectReject(tests, root, "primitive_pointer", R"NS(
public class Program
{
    public static int Main() { int* value = stackalloc int[1]; destroy value; return 0; }
}
)NS",
                 "destroy requires a class/interface reference or a pointer to a "
                 "concrete class or struct");

    expectReject(tests, root, "raii", R"NS(
public class Resource { ~Resource() { } }
public class Program
{
    public static int Main() { raii Resource value = new Resource(); destroy value; return 0; }
}
)NS",
                 "explicit destroy of a RAII local is forbidden");

    expectReject(tests, root, "owned_parameter", R"NS(
public class Resource { ~Resource() { } }
public class Program
{
    private static void Close(owned Resource value) { destroy value; }
    public static int Main() { return 0; }
}
)NS",
                 "explicit destroy of an owned value is forbidden");

    return tests.resultKeeping(root);
}
