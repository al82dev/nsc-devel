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
    writeFile(root / "Placement.nsproj",
        "<Project><PropertyGroup><Name>PlacementItem318</Name></PropertyGroup>"
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
    const std::string project = (caseRoot / "Placement.nsproj").string();
    const std::array<std::string_view, 3> arguments = {
        "--dump-project-oop-hir", project, "-O0"};
    std::string output;
    std::string error;
    tests.expect(runDriver(arguments, output, error) != nativesharp::ExitCode::Success,
        std::string(name) + " is rejected");
    tests.expect(error.find(diagnostic) != std::string::npos,
        std::string(name) + " reports the placement contract (" + error + ")");
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root =
        std::filesystem::path(NSC_BINARY_DIR) / "item318_placement_construction_lowering_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    const auto validRoot = root / "valid";
    writeProject(validRoot, R"NS(
public struct Cell
{
    public int Left;
    public int Right;

    public Cell(int left, int right)
    {
        Left = left;
        Right = right;
    }
}

public struct ZeroCell
{
    public int First;
    public int Second;
}

public class Gadget
{
    public int Value;
    public Gadget(int value) { Value = value; }
    public int Read() { return Value; }
}

public class Program
{
    private static Cell* Select(Cell* storage, ref int trace)
    {
        trace = trace * 10 + 1;
        return storage;
    }

    private static int Mark(ref int trace, int digit)
    {
        trace = trace * 10 + digit;
        return digit;
    }

    public static int Main()
    {
        int trace = 0;
        Cell* storage = stackalloc Cell[1];
        storage[0].Left = 99;
        storage[0].Right = 99;
        Cell* placed = new (Select(storage, ref trace)) Cell(
            Mark(ref trace, 2), Mark(ref trace, 3));
        if (trace != 123 || placed != storage) { return 1; }
        if (placed->Left != 2 || placed->Right != 3) { return 2; }
        placed->Left += 37;

        ZeroCell* zeroStorage = stackalloc ZeroCell[1];
        zeroStorage[0].First = 8;
        zeroStorage[0].Second = 9;
        ZeroCell* zero = new (zeroStorage) ZeroCell();
        if (zero->First != 0 || zero->Second != 0) { return 3; }

        long* classWords = stackalloc long[4];
        Gadget* classStorage = (Gadget*)classWords;
        Gadget gadget = new (classStorage) Gadget(41);
        if (gadget.Read() != 41 || gadget.Value != 41) { return 4; }
        return placed->Left == 39 ? 0 : 5;
    }
}
)NS");

    const std::string project = (validRoot / "Placement.nsproj").string();
    std::string output;
    std::string error;

    const std::array<std::string_view, 3> bodyArguments = {
        "--dump-project-method-bodies", project, "-O0"};
    const bool bodies = runDriver(bodyArguments, output, error) == ExitCode::Success;
    tests.expect(bodies, "placement construction passes semantic analysis (" + error + ")");

    const std::array<std::string_view, 3> hirArguments = {
        "--dump-project-oop-hir", project, "-O0"};
    output.clear();
    error.clear();
    const bool hir = runDriver(hirArguments, output, error) == ExitCode::Success;
    tests.expect(hir, "placement construction lowers to O0 HIR (" + error + ")");
    if (hir)
    {
        writeFile(validRoot / "Placement.hir.txt", output);
        tests.expect(output.find("placement-construction type=type:PlacementItem318::Cell*") !=
                std::string::npos,
            "HIR retains placement construction as a pointer result");
    }

    const std::array<std::string_view, 3> mirArguments = {
        "--dump-project-oop-mir", project, "-O0"};
    output.clear();
    error.clear();
    const bool mir = runDriver(mirArguments, output, error) == ExitCode::Success;
    tests.expect(mir, "placement construction lowers to O0 MIR (" + error + ")");
    if (mir)
    {
        writeFile(validRoot / "Placement.mir.txt", output);
        tests.expect(output.find("placement-construct type=type:PlacementItem318::Cell*") !=
                std::string::npos &&
                output.find("bytes=8") != std::string::npos &&
                output.find("storage=supplied") != std::string::npos,
            "MIR freezes the destination layout without an allocation plan");
    }

    const std::array<std::string_view, 3> llvmArguments = {
        "--dump-project-oop-llvm", project, "-O0"};
    output.clear();
    error.clear();
    const bool llvm = runDriver(llvmArguments, output, error) == ExitCode::Success;
    tests.expect(llvm, "placement construction emits verifier-clean O0 LLVM (" + error + ")");
    if (llvm)
    {
        writeFile(validRoot / "Placement.ll", output);
        tests.expect(output.find("placement.notnull") != std::string::npos &&
                output.find("call ptr @__nativesharp_alloc") == std::string::npos,
            "LLVM guards supplied storage and performs no heap allocation call");
    }

    const std::string executable = (validRoot / "Placement.exe").string();
    const std::array<std::string_view, 5> executableArguments = {
        "-O0", "--emit-project-oop-exe", project, "-o", executable};
    output.clear();
    error.clear();
    const bool emitted = runDriver(executableArguments, output, error) == ExitCode::Success;
    tests.expect(emitted, "placement fixture links at O0 (" + error + ")");
    if (emitted)
    {
        const std::string command = "\"" + executable + "\"";
        tests.expect(std::system(command.c_str()) == 0,
            "destination and arguments evaluate once in order and constructors initialize in place");
    }

    expectReject(tests, root, "wrong_storage_type", R"NS(
public struct Cell { public int Value; public Cell(int value) { Value = value; } }
public class Program
{
    public static int Main()
    {
        int* storage = stackalloc int[2];
        Cell* cell = new (storage) Cell(1);
        return 0;
    }
}
)NS", "requires destination storage of exact type");

    expectReject(tests, root, "non_constructible_type", R"NS(
public class Program
{
    public static int Main()
    {
        int* storage = stackalloc int[1];
        int* value = new (storage) int();
        return 0;
    }
}
)NS", "requires an unmodified class or struct type");

    expectReject(tests, root, "bad_constructor", R"NS(
public struct Cell { public Cell(int value) { } }
public class Program
{
    public static int Main()
    {
        Cell* storage = stackalloc Cell[1];
        Cell* cell = new (storage) Cell(1, 2);
        return 0;
    }
}
)NS", "no accessible constructor accepts");

    return tests.resultKeeping(root);
}
