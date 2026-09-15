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
    writeFile(root / "StackClass.nsproj",
        "<Project><PropertyGroup><Name>StackClassItem323</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Main.ns", source);
}

void expectHeapBacked(
    nativesharp::tests::TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source)
{
    const auto caseRoot = root / std::string(name);
    writeProject(caseRoot, source);
    const std::string project = (caseRoot / "StackClass.nsproj").string();
    const std::array<std::string_view, 3> arguments = {
        "--dump-project-oop-hir", project, "-O0"};
    std::string output;
    std::string error;
    const bool accepted = runDriver(arguments, output, error) == nativesharp::ExitCode::Success;
    tests.expect(accepted, std::string(name) + " remains a valid heap program (" + error + ")");
    if (accepted)
    {
        tests.expect(output.find("object-construction") != std::string::npos &&
                         output.find("stack-promoted=true") == std::string::npos &&
                         output.find("storage-ownership=owned") != std::string::npos,
            std::string(name) + " conservatively retains owned heap storage");

        const std::array<std::string_view, 3> llvmArguments = {
            "--dump-project-oop-llvm", project, "-O0"};
        output.clear();
        error.clear();
        const bool llvm = runDriver(llvmArguments, output, error) == nativesharp::ExitCode::Success;
        tests.expect(llvm, std::string(name) + " emits heap-backed LLVM (" + error + ")");
        if (llvm)
        {
            tests.expect(output.find("call ptr @__nativesharp_alloc") != std::string::npos,
                std::string(name) + " physically retains its allocator call");
        }
    }
}

void expectNoHeapReject(
    nativesharp::tests::TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source)
{
    const auto caseRoot = root / std::string(name);
    writeProject(caseRoot, source);
    const std::string project = (caseRoot / "StackClass.nsproj").string();
    const std::array<std::string_view, 3> arguments = {
        "--dump-project-oop-hir", project, "-O0"};
    std::string output;
    std::string error;
    tests.expect(runDriver(arguments, output, error) != nativesharp::ExitCode::Success,
        std::string(name) + " cannot claim noheap after the proof declines promotion");
    tests.expect(error.find(
                     "cannot contain direct heap allocation 'object-construction'") !=
                     std::string::npos,
        std::string(name) + " retains the item 321 heap diagnostic (" + error + ")");
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root =
        std::filesystem::path(NSC_BINARY_DIR) / "item323_stack_class_escape_analysis_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    const auto validRoot = root / "valid";
    writeProject(validRoot, R"NS(
public class Cell
{
    public int Left;
    public int Right;
}

public class Program
{
    public static noheap int Work()
    {
        raii Cell cell = new Cell();
        cell.Left = 40;
        cell.Right += 2;
        return cell.Left + cell.Right;
    }

    public static int Main() { return Work() == 42 ? 0 : 1; }
}
)NS");

    const std::string project = (validRoot / "StackClass.nsproj").string();
    std::string output;
    std::string error;

    const std::array<std::string_view, 3> hirArguments = {
        "--dump-project-oop-hir", project, "-O0"};
    const bool hir = runDriver(hirArguments, output, error) == ExitCode::Success;
    tests.expect(hir, "nonescaping class construction is valid in noheap HIR (" + error + ")");
    if (hir)
    {
        writeFile(validRoot / "StackClass.hir.txt", output);
        tests.expect(output.find("local-declaration type=type:StackClassItem323::Cell") !=
                         std::string::npos &&
                         output.find("object-construction type=type:StackClassItem323::Cell") !=
                         std::string::npos,
            "HIR retains the source local and class construction identities");
        tests.expect(output.find("stack-promoted=true") != std::string::npos &&
                         output.find("object-ownership=owned storage-ownership=none") !=
                         std::string::npos,
            "HIR records owned object lifetime over frame-owned storage");
    }

    const std::array<std::string_view, 3> mirArguments = {
        "--dump-project-oop-mir", project, "-O0"};
    output.clear();
    error.clear();
    const bool mir = runDriver(mirArguments, output, error) == ExitCode::Success;
    tests.expect(mir, "stack promotion lowers to O0 MIR (" + error + ")");
    if (mir)
    {
        writeFile(validRoot / "StackClass.mir.txt", output);
        tests.expect(output.find("schema=1.6") != std::string::npos,
            "MIR 1.6 publishes stack-promoted class semantics");
        tests.expect(output.find("allocate-and-construct type=type:StackClassItem323::Cell") !=
                         std::string::npos &&
                         output.find("stack-promoted=true") != std::string::npos &&
                         output.find("storage-ownership=none") != std::string::npos,
            "MIR carries the proof and canonical object layout");
    }

    const std::array<std::string_view, 3> llvmArguments = {
        "--dump-project-oop-llvm", project, "-O0"};
    output.clear();
    error.clear();
    const bool llvm = runDriver(llvmArguments, output, error) == ExitCode::Success;
    tests.expect(llvm, "stack promotion emits verifier-clean O0 LLVM (" + error + ")");
    if (llvm)
    {
        writeFile(validRoot / "StackClass.ll", output);
        tests.expect(output.find(".object = alloca [16 x i8]") != std::string::npos,
            "LLVM hoists a correctly sized and aligned object buffer into the function frame");
        tests.expect(output.find("call ptr @__nativesharp_alloc") == std::string::npos,
            "the promoted program performs no heap allocation call");
        tests.expect(output.find("raii.object.entry.addr") != std::string::npos,
            "RAII closes only the object lifetime and never frees stack storage");
    }

    const std::string executable = (validRoot / "StackClass.exe").string();
    const std::array<std::string_view, 5> executableArguments = {
        "-O0", "--emit-project-oop-exe", project, "-o", executable};
    output.clear();
    error.clear();
    const bool emitted = runDriver(executableArguments, output, error) == ExitCode::Success;
    tests.expect(emitted, "stack-promoted fixture links at O0 (" + error + ")");
    if (emitted)
    {
        const std::string command = "\"" + executable + "\"";
        tests.expect(std::system(command.c_str()) == 0,
            "field stores and loads execute correctly from the promoted object");
    }

    expectHeapBacked(tests, root, "observable_identity", R"NS(
public class Box { public int Value; }
public class Program
{
    public static int Main()
    {
        raii Box box = new Box();
        return box == null ? 0 : box.Value;
    }
}
)NS");

    expectHeapBacked(tests, root, "method_receiver", R"NS(
public class Box { public int Value; public int Read() { return Value; } }
public class Program
{
    public static int Main()
    {
        raii Box box = new Box();
        return box.Read();
    }
}
)NS");

    expectHeapBacked(tests, root, "explicit_constructor", R"NS(
public class Box { public int Value; public Box() { } }
public class Program
{
    public static int Main()
    {
        raii Box box = new Box();
        return box.Value;
    }
}
)NS");

    expectNoHeapReject(tests, root, "escaping_noheap_receiver", R"NS(
public class Box { public int Value; public noheap int Read() { return Value; } }
public class Program
{
    public static noheap int Work()
    {
        raii Box box = new Box();
        return box.Read();
    }
    public static int Main() { return 0; }
}
)NS");

    return tests.resultKeeping(root);
}
