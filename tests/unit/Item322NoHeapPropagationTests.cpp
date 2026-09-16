#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryMetadata.h"

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
    writeFile(root / "NoHeap.nsproj",
        "<Project><PropertyGroup><Name>NoHeapItem322</Name></PropertyGroup>"
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
    const std::string project = (caseRoot / "NoHeap.nsproj").string();
    const std::array<std::string_view, 3> arguments = {
        "--dump-project-oop-hir", project, "-O0"};
    std::string output;
    std::string error;
    tests.expect(runDriver(arguments, output, error) != nativesharp::ExitCode::Success,
        std::string(name) + " is rejected");
    tests.expect(error.find(diagnostic) != std::string::npos,
        std::string(name) + " reports its noheap propagation rule (" + error + ")");
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item322_noheap_propagation_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    const auto validRoot = root / "valid";
    writeProject(validRoot, R"NS(
public struct Cell
{
    public int Value;
    public noheap Cell() { Value = 0; }
}

public class RootObject
{
    public noheap RootObject() { }
}

public class Program
{
    public static noheap int Leaf(int value) { return value + 1; }
    public static noheap int Middle(Cell* storage)
    {
        Cell* cell = new (storage) Cell();
        cell->Value = 6;
        int result = Leaf(cell->Value);
        destroy cell;
        return result;
    }
    public static noheap int Root(Cell* storage) { return Middle(storage); }
    public static int Main() { return 0; }
}
)NS");
    const std::string validProject = (validRoot / "NoHeap.nsproj").string();
    std::string output;
    std::string error;
    const std::array<std::string_view, 3> hirArguments = {
        "--dump-project-oop-hir", validProject, "-O0"};
    const bool hir = runDriver(hirArguments, output, error) == ExitCode::Success;
    tests.expect(hir, "noheap call chains and an explicit noheap placement constructor lower (" + error + ")");
    if (hir)
    {
        tests.expect(output.find("noheap=true") != std::string::npos &&
                         output.find("placement-construction") != std::string::npos,
            "HIR transports the callable contract and placement operation");
    }

    const std::array<std::string_view, 3> mirArguments = {
        "--dump-project-oop-mir", validProject, "-O0"};
    output.clear();
    error.clear();
    const bool mir = runDriver(mirArguments, output, error) == ExitCode::Success;
    tests.expect(mir, "noheap contracts lower to MIR (" + error + ")");
    if (mir)
    {
        tests.expect(output.find("schema=1.10") != std::string::npos &&
                         output.find("noheap=true") != std::string::npos,
            "current MIR transports the noheap function flag");
    }

    expectReject(tests, root, "direct_call", R"NS(
public class Program
{
    private static int Ordinary() { return 1; }
    private static noheap int Guarded() { return Ordinary(); }
    public static int Main() { return 0; }
}
)NS", "cannot call non-noheap callable 'Ordinary()'");

    expectReject(tests, root, "placement_constructor", R"NS(
public struct Cell { public Cell() { } }
public class Program
{
    private static noheap Cell* Build(Cell* storage) { return new (storage) Cell(); }
    public static int Main() { return 0; }
}
)NS", "cannot call non-noheap constructor");

    expectReject(tests, root, "implicit_constructor", R"NS(
public struct Cell { public int Value; }
public class Program
{
    private static noheap Cell* Build(Cell* storage) { return new (storage) Cell(); }
    public static int Main() { return 0; }
}
)NS", "cannot invoke an implicit constructor without a noheap contract");

    expectReject(tests, root, "base_initializer", R"NS(
public class Base { public Base() { } }
public class Derived : Base { public noheap Derived() : base() { } }
public class Program { public static int Main() { return 0; } }
)NS", "cannot call non-noheap constructor");

    expectReject(tests, root, "field_initializer", R"NS(
public class Box { }
public struct Holder
{
    public Box Value = new Box();
    public noheap Holder() { }
}
public class Program
{
    private static noheap Holder* Build(Holder* storage) { return new (storage) Holder(); }
    public static int Main() { return 0; }
}
)NS", "cannot contain direct heap allocation");

    expectReject(tests, root, "virtual_override", R"NS(
public class Base { public virtual noheap int Work() { return 1; } }
public class Derived : Base { public override int Work() { return 2; } }
public class Program { public static int Main() { return 0; } }
)NS", "cannot drop the inherited noheap contract");

    expectReject(tests, root, "interface_implementation", R"NS(
public interface IWorker { public noheap int Work(); }
public class Worker : IWorker { public int Work() { return 1; } }
public class Program { public static int Main() { return 0; } }
)NS", "cannot implement noheap interface contract");

    expectReject(tests, root, "delegate_invoke", R"NS(
public delegate int Worker();
public class Program
{
    private static noheap int Invoke(Worker worker) { return worker(); }
    public static int Main() { return 0; }
}
)NS", "cannot invoke an indirect target without a noheap contract");

    const auto producer = root / "producer";
    writeFile(producer / "NoHeapProducer.nsproj",
        "<Project><PropertyGroup><Name>NoHeapProducer</Name><OutputType>StaticLibrary</OutputType>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(producer / "Api.ns", R"NS(
namespace CrossNoHeap
{
    public class Api
    {
        public static noheap int Value() { return 42; }
    }
}
)NS");
    const std::string producerProject = (producer / "NoHeapProducer.nsproj").string();
    const std::string producerLibrary = (producer / "NoHeapProducer.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "-O0", "--emit-project-oop-lib", producerProject, "-o", producerLibrary};
    output.clear();
    error.clear();
    tests.expect(runDriver(producerArguments, output, error) == ExitCode::Success,
        "a noheap producer emits its library and current NSMETA (" + error + ")");

    nativesharp::interop::LibraryMetadataCodec codec;
    nativesharp::interop::LibraryMetadata metadata;
    std::string metadataError;
    const auto metadataPath = producer / "NoHeapProducer.nsmeta";
    const bool metadataRead = codec.read(metadataPath, metadata, metadataError);
    bool metadataNoHeap = false;
    if (metadataRead)
    {
        for (const auto& member : metadata.members)
        {
            if (member.name == "Value" &&
                (member.flags & nativesharp::interop::kMetadataMemberNoHeap) != 0U)
                metadataNoHeap = true;
        }
    }
    tests.expect(metadataRead &&
                     metadata.formatMinor == nativesharp::interop::kMetadataFormatMinor && metadataNoHeap,
        "current NSMETA preserves the 1.16 public noheap bit (" + metadataError + ")");
    if (metadataRead)
    {
        metadata.formatMinor = 15U;
        const auto downlevelPath = producer / "NoHeapProducer-1.15.nsmeta";
        tests.expect(codec.writeModel(metadata, downlevelPath, metadataError),
            "the historical NSMETA 1.15 form can still be emitted");
        nativesharp::interop::LibraryMetadata downlevel;
        const bool downlevelRead = codec.read(downlevelPath, downlevel, metadataError);
        bool retainedNoHeap = false;
        if (downlevelRead)
        {
            for (const auto& member : downlevel.members)
                retainedNoHeap = retainedNoHeap ||
                    (member.flags & nativesharp::interop::kMetadataMemberNoHeap) != 0U;
        }
        tests.expect(downlevelRead && !retainedNoHeap,
            "NSMETA 1.15 output strips the 1.16 noheap bit");
    }

    const auto consumer = root / "consumer";
    writeFile(consumer / "NoHeapConsumer.nsproj",
        "<Project><PropertyGroup><Name>NoHeapConsumer</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup><ItemGroup>"
        "<LibraryReference Include=\"" + producerLibrary + "\" Metadata=\"" +
        metadataPath.string() + "\" /></ItemGroup></Project>");
    writeFile(consumer / "Main.ns", R"NS(
using CrossNoHeap;
public class Program
{
    public static noheap int Read() { return Api.Value(); }
    public static int Main() { return 0; }
}
)NS");
    const std::string consumerProject = (consumer / "NoHeapConsumer.nsproj").string();
    const std::array<std::string_view, 3> consumerArguments = {
        "--dump-project-oop-hir", consumerProject, "-O0"};
    output.clear();
    error.clear();
    tests.expect(runDriver(consumerArguments, output, error) == ExitCode::Success,
        "LibraryReference reconstructs the noheap contract for a guarded consumer (" + error + ")");

    return tests.resultKeeping(root);
}
