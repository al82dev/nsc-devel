#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryMetadata.h"

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
    writeFile(root / "ReferenceIndexers.nsproj",
        "<Project><PropertyGroup><Name>ReferenceIndexersItem314</Name></PropertyGroup>"
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
    const std::string project = (caseRoot / "ReferenceIndexers.nsproj").string();
    const std::array<std::string_view, 3> arguments = {
        "--dump-project-oop-hir", project, "-O0"};
    std::string output;
    std::string error;
    tests.expect(runDriver(arguments, output, error) != nativesharp::ExitCode::Success,
        std::string(name) + " is rejected");
    tests.expect(error.find(diagnostic) != std::string::npos,
        std::string(name) + " reports the ref-indexer contract (" + error + ")");
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item314_reference_indexers_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    const auto validRoot = root / "valid";
    writeProject(validRoot, R"NS(
public interface ICell
{
    ref int this[int index] { get; }
}

public class Cell : ICell
{
    public int Value;

    public virtual ref int this[int index]
    {
        get { return ref Value; }
    }
}

public class DerivedCell : Cell
{
    public int Other;

    public override ref int this[int index]
    {
        get { return ref Other; }
    }
}

public class Program
{
    public static int Main()
    {
        raii Cell direct = new Cell();
        direct[0] = 10;
        direct[1] += 2;
        ref int alias = ref direct[2];
        alias++;
        if (direct.Value != 13 || direct[3] != 13) { return 1; }

        raii Cell virtualCell = new DerivedCell();
        virtualCell[0] = 20;
        if (((DerivedCell)virtualCell).Other != 20) { return 2; }

        borrowed ICell interfaceCell = virtualCell;
        interfaceCell[0] = 30;
        return ((DerivedCell)virtualCell).Other == 30 ? 0 : 3;
    }
}
)NS");

    const std::string project = (validRoot / "ReferenceIndexers.nsproj").string();
    std::string output;
    std::string error;

    const std::array<std::string_view, 3> membersArguments = {
        "--dump-project-members", project, "-O0"};
    const bool members = runDriver(membersArguments, output, error) == ExitCode::Success;
    tests.expect(members, "ref indexers build semantic members (" + error + ")");
    if (members)
    {
        tests.expect(output.find("get_Item") != std::string::npos &&
                output.find("ref-return=true") != std::string::npos,
            "the synthesized indexer getter carries the ref-return mode");
        tests.expect(output.find("set_Item") == std::string::npos,
            "a ref indexer remains get-only without a synthesized setter");
    }

    const std::array<std::string_view, 3> hirArguments = {
        "--dump-project-oop-hir", project, "-O0"};
    output.clear();
    error.clear();
    const bool hir = runDriver(hirArguments, output, error) == ExitCode::Success;
    tests.expect(hir, "ref indexers lower to O0 HIR (" + error + ")");
    if (hir)
    {
        tests.expect(output.find("reference-result=true") != std::string::npos,
            "indexer calls preserve returned-address identity in HIR");
        tests.expect(output.find("compound-store") != std::string::npos,
            "compound assignment uses the returned address instead of a setter pair");
    }

    const std::array<std::string_view, 3> mirArguments = {
        "--dump-project-oop-mir", project, "-O0"};
    output.clear();
    error.clear();
    const bool mir = runDriver(mirArguments, output, error) == ExitCode::Success;
    tests.expect(mir && output.find("reference-result=true") != std::string::npos,
        "MIR transports the ref-indexer result contract (" + error + ")");

    const std::array<std::string_view, 3> llvmArguments = {
        "--dump-project-oop-llvm", project, "-O0"};
    output.clear();
    error.clear();
    const bool llvm = runDriver(llvmArguments, output, error) == ExitCode::Success;
    tests.expect(llvm, "ref indexers emit verifier-ready O0 LLVM (" + error + ")");
    if (llvm)
    {
        writeFile(validRoot / "ReferenceIndexers.ll", output);
        tests.expect(output.find("define ptr") != std::string::npos &&
                output.find("store i32") != std::string::npos,
            "LLVM returns pointer addresses and stores through them");
    }

    const std::string executable = (validRoot / "ReferenceIndexers.exe").string();
    const std::array<std::string_view, 5> executableArguments = {
        "-O0", "--emit-project-oop-exe", project, "-o", executable};
    output.clear();
    error.clear();
    const bool emitted = runDriver(executableArguments, output, error) == ExitCode::Success;
    tests.expect(emitted, "ref-indexer fixture links at O0 (" + error + ")");
    if (emitted)
    {
        const std::string command = "\"" + executable + "\"";
        tests.expect(std::system(command.c_str()) == 0,
            "direct, virtual, and interface ref indexers mutate selected storage");
    }

    expectReject(tests, root, "setter", R"NS(
public class Cell
{
    private int Value;
    public ref int this[int index]
    {
        get { return ref Value; }
        set { Value = value; }
    }
}
public class Program { public static int Main() { return 0; } }
)NS", "get-only and cannot declare a setter");

    expectReject(tests, root, "ordinary_getter_address", R"NS(
public class Cell
{
    public int this[int index] { get { return index; } }
}
public class Program
{
    public static int Main()
    {
        raii Cell cell = new Cell();
        ref int alias = ref cell[0];
        return alias;
    }
}
)NS", "must be a mutable addressable storage location");

    expectReject(tests, root, "interface_mode_mismatch", R"NS(
public interface ICell { ref int this[int index] { get; } }
public class Cell : ICell
{
    public int this[int index] { get { return index; } }
}
public class Program { public static int Main() { return 0; } }
)NS", "does not implement interface method");

    const auto producer = root / "producer";
    writeFile(producer / "IndexProducer.nsproj",
        "<Project><PropertyGroup><Name>IndexProducer</Name><OutputType>StaticLibrary</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(producer / "Cell.ns", R"NS(
namespace CrossIndex
{
    public class Cell
    {
        private int Value;
        public ref int this[int index] { get { return ref Value; } }
    }
}
)NS");
    const std::string producerProject = (producer / "IndexProducer.nsproj").string();
    const std::string producerLibrary = (producer / "IndexProducer.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "-O0", "--emit-project-oop-lib", producerProject, "-o", producerLibrary};
    output.clear();
    error.clear();
    tests.expect(runDriver(producerArguments, output, error) == ExitCode::Success,
        "a public ref indexer emits a static library and metadata (" + error + ")");

    nativesharp::interop::LibraryMetadata metadata;
    nativesharp::interop::LibraryMetadataCodec codec;
    std::string metadataError;
    const auto metadataPath = producer / "IndexProducer.nsmeta";
    const bool metadataRead = codec.read(metadataPath, metadata, metadataError);
    bool getterBit = false;
    if (metadataRead)
    {
        for (const auto& member : metadata.members)
        {
            getterBit = getterBit || (member.name == "get_Item" &&
                (member.flags & nativesharp::interop::kMetadataMemberReferenceReturn) != 0U);
        }
    }
    tests.expect(metadataRead && getterBit,
        "NSMETA 1.15 marks the public ref-indexer getter (" + metadataError + ")");

    const auto consumer = root / "consumer";
    writeFile(consumer / "IndexConsumer.nsproj",
        "<Project><PropertyGroup><Name>IndexConsumer</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup><ItemGroup>"
        "<LibraryReference Include=\"" + (producer / "IndexProducer.lib").generic_string() +
        "\" Metadata=\"" + metadataPath.generic_string() + "\" /></ItemGroup></Project>");
    writeFile(consumer / "Main.ns", R"NS(
using CrossIndex;
public class Program
{
    public static int Main()
    {
        raii Cell cell = new Cell();
        cell[0] = 77;
        return cell[1] == 77 ? 0 : 1;
    }
}
)NS");
    const std::string consumerProject = (consumer / "IndexConsumer.nsproj").string();
    const std::string consumerExecutable = (consumer / "IndexConsumer.exe").string();
    const std::array<std::string_view, 5> consumerArguments = {
        "-O0", "--emit-project-oop-exe", consumerProject, "-o", consumerExecutable};
    output.clear();
    error.clear();
    const bool consumerEmitted = runDriver(consumerArguments, output, error) == ExitCode::Success;
    tests.expect(consumerEmitted,
        "LibraryReference reconstructs the get-only ref indexer (" + error + ")");
    if (consumerEmitted)
    {
        const std::string command = "\"" + consumerExecutable + "\"";
        tests.expect(std::system(command.c_str()) == 0,
            "a precompiled ref indexer preserves its returned-address ABI");
    }

    return tests.resultKeeping(root);
}
