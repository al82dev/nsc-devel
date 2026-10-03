#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
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
    writeFile(root / "NoHeap.nsproj",
              "<Project><PropertyGroup><Name>NoHeapItem321</Name></PropertyGroup>"
              "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Main.ns", source);
}

void expectAllocationReject(nativesharp::tests::TestContext& tests,
                            const std::filesystem::path& root, std::string_view name,
                            std::string_view body, std::string_view allocationKind)
{
    const auto caseRoot = root / std::string(name);
    const std::string source =
        "public delegate int Worker();\n"
        "public class Box { public int Value; }\n"
        "public class Program { public static noheap int Work() { " + std::string(body) +
        " } public static int Main() { return 0; } }\n";
    writeProject(caseRoot, source);
    const std::string project = (caseRoot / "NoHeap.nsproj").string();
    const std::array<std::string_view, 3> arguments = {
        "--dump-project-oop-hir", project, "-O0"};
    std::string output;
    std::string error;
    tests.expect(runDriver(arguments, output, error) != nativesharp::ExitCode::Success,
                 std::string(name) + " is rejected in a noheap method");
    tests.expect(error.find("NS3030") != std::string::npos &&
                     error.find("noheap callable 'Work' cannot contain direct heap allocation '") !=
                         std::string::npos &&
                     error.find(allocationKind) != std::string::npos,
                 std::string(name) + " reports NS3030 and names its direct heap operation (" +
                     error + ")");
}
} // namespace

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item321_noheap_context_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    const auto validRoot = root / "valid";
    writeProject(validRoot, R"NS(
public struct Cell { public int Value; public noheap Cell() { } }

public class Program
{
    public static noheap int Work()
    {
        Cell* cells = stackalloc Cell[1];
        Cell* cell = new (cells) Cell();
        cell->Value = 7;
        destroy cell;
        return 7;
    }

    public static int Main() { return Work() == 7 ? 0 : 1; }
}
)NS");

    const std::string project = (validRoot / "NoHeap.nsproj").string();
    std::string output;
    std::string error;

    const std::array<std::string_view, 3> memberArguments = {
        "--dump-project-members", project, "-O0"};
    const bool members = runDriver(memberArguments, output, error) == ExitCode::Success;
    tests.expect(members, "noheap is accepted as an ordinary method contract (" + error + ")");
    if (members)
    {
        const std::size_t method = output.find(" method Work ");
        const std::size_t lineEnd = output.find('\n', method);
        const std::string_view methodLine = method == std::string::npos
            ? std::string_view{}
            : std::string_view(output).substr(method, lineEnd - method);
        tests.expect(methodLine.find("noheap=true") != std::string::npos &&
                         methodLine.find("effects=16") != std::string::npos,
                     "the member model exposes noheap through the canonical effect set");
    }

    const std::array<std::string_view, 3> hirArguments = {
        "--dump-project-oop-hir", project, "-O0"};
    output.clear();
    error.clear();
    const bool hir = runDriver(hirArguments, output, error) == ExitCode::Success;
    tests.expect(hir, "stackalloc and placement remain valid in a noheap method (" + error + ")");
    if (hir)
    {
        tests.expect(output.find("stack-allocation") != std::string::npos &&
                         output.find("placement-construction") != std::string::npos,
                     "noheap permits caller-provided and stack storage");
    }

    expectAllocationReject(tests, root, "class_new", "Box value = new Box(); return 0;",
                           "object-construction");
    expectAllocationReject(tests, root, "raw_new", "int* value = new int; return 0;",
                           "raw-allocation");
    expectAllocationReject(tests, root, "array_new", "int[] value = new int[4]; return 0;",
                           "array-allocation");
    expectAllocationReject(tests, root, "string_concat",
                           "raii string value = \"a\" + \"b\"; return value.Length;",
                           "string-concat");
    expectAllocationReject(tests, root, "lambda",
                           "raii Worker value = () => 1; return 0;", "lambda-create");

    const auto duplicateRoot = root / "duplicate";
    writeProject(duplicateRoot, R"NS(
public class Program
{
    public static noheap noheap int Work() { return 0; }
    public static int Main() { return 0; }
}
)NS");
    const std::string duplicateProject = (duplicateRoot / "NoHeap.nsproj").string();
    const std::array<std::string_view, 3> duplicateArguments = {
        "--dump-project-oop-hir", duplicateProject, "-O0"};
    output.clear();
    error.clear();
    tests.expect(runDriver(duplicateArguments, output, error) != ExitCode::Success,
                 "duplicate noheap is rejected");
    tests.expect(error.find("duplicate noheap modifier") != std::string::npos,
                 "duplicate noheap reports the modifier contract");

    return tests.resultKeeping(root);
}
