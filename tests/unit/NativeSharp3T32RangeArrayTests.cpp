#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ranges>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::ExitCode;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

ExitCode runDriver(std::span<const std::string_view> arguments, std::string& out, std::string& err)
{
    nativesharp::driver::Driver driver;
    std::ostringstream output;
    std::ostringstream error;
    const ExitCode code = driver.run(arguments, output, error);
    out = output.str();
    err = error.str();
    return code;
}

void writeProject(const std::filesystem::path& directory, std::string_view name,
    std::string_view outputType = {})
{
    const std::string output = outputType.empty()
        ? std::string{}
        : "<OutputType>" + std::string(outputType) + "</OutputType>";
    writeFile(directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) + "</Name>" + output +
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/></ItemGroup></Project>");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t32_range_arrays";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) / "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    const std::filesystem::path producer = root / "producer";
    writeProject(producer, "RangeProducer", "StaticLibrary");
    writeFile(producer / "Api.ns", R"NS(
namespace RangeApi
{
    public class Matrices
    {
        public static int Sum(int[,] value)
        {
            return value[0, 0] + value[0, 1] + value[1, 0] + value[1, 1];
        }
    }
}
)NS");
    const std::string producerProject = (producer / "RangeProducer.nsproj").string();
    const std::string producerLibrary = (producer / "RangeProducer.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "-O0", "--emit-project-oop-lib", producerProject, "-o", producerLibrary};
    std::string output;
    std::string error;
    tests.expect(runDriver(producerArguments, output, error) == ExitCode::Success,
        "T32 producer emits a rectangular-array library at O0 (" + error + output + ")");

    nativesharp::interop::LibraryMetadata metadata;
    nativesharp::interop::LibraryMetadataCodec codec;
    error.clear();
    const bool metadataRead = codec.read(producer / "RangeProducer.nsmeta", metadata, error);
    const auto sum = std::ranges::find_if(metadata.members,
        [](const auto& member) { return member.name == "Sum"; });
    tests.expect(metadataRead && sum != metadata.members.end() && sum->parameters.size() == 1 &&
            sum->parameters[0].typeKey == "builtin:int[,]",
        "NSMETA preserves rank-two identity instead of degrading it to jagged (" + error + ")");

    const std::filesystem::path consumer = root / "consumer";
    writeFile(consumer / "RangeConsumer.nsproj",
        "<Project><PropertyGroup><Name>RangeConsumer</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\"/></ItemGroup><ItemGroup>"
        "<LibraryReference Include=\"" + (producer / "RangeProducer.lib").generic_string() +
        "\" Metadata=\"" + (producer / "RangeProducer.nsmeta").generic_string() +
        "\"/></ItemGroup></Project>");
    writeFile(consumer / "Main.ns", R"NS(
using RangeApi;
public class Payload
{
    public static int Drops = 0;
    public int Value;
    public Payload(int value) { Value = value; }
    ~Payload() { Drops = Drops + 1; }
}
public class Program
{
    public static int Main()
    {
        int[,] matrix = new int[2, 2];
        matrix[0, 0] = 10;
        matrix[0, 1] = 11;
        matrix[1, 0] = 9;
        matrix[1, 1] = 12;
        if (matrix.Length != 4 || Matrices.Sum(matrix) != 42) { return 1; }
        int[,,] cube = new int[2, 2, 2];
        cube[1, 0, 1] = 42;
        if (cube.Length != 8 || cube[1, 0, 1] != 42) { return 2; }
        Payload[,] owners = new Payload[2, 2];
        owners[0, 0] = new Payload(1);
        owners[1, 1] = new Payload(2);
        delete[] owners;
        if (Payload.Drops != 2) { return 3; }
        delete[] cube;
        delete[] matrix;
        return 0;
    }
}
)NS");
    const std::string consumerProject = (consumer / "RangeConsumer.nsproj").string();
    const std::string executable = (consumer / "RangeConsumer.exe").string();
    const std::array<std::string_view, 5> consumerArguments = {
        "-O0", "--emit-project-oop-exe", consumerProject, "-o", executable};
    output.clear();
    error.clear();
    const bool emitted = runDriver(consumerArguments, output, error) == ExitCode::Success;
    tests.expect(emitted,
        "cross-module rank-two and local rank-three arrays compile/link at O0 (" + error + output + ")");
    if (emitted)
    {
        const std::string command = "cmd.exe /d /s /c \"\"" + executable + "\"\"";
        tests.expect(std::system(command.c_str()) == 0,
            "row-major indexing, total Length and rectangular destruction execute correctly");
    }

    const std::array<std::string_view, 3> hirArguments = {
        "--dump-project-oop-hir", consumerProject, "-O0"};
    output.clear();
    error.clear();
    tests.expect(runDriver(hirArguments, output, error) == ExitCode::Success &&
            output.find("range-array-allocation type=builtin:int[,] element=builtin:int rank=2") !=
                std::string::npos &&
            output.find("range-array-element-address type=builtin:int element=builtin:int rank=2") !=
                std::string::npos,
        "HIR exposes rectangular allocation/access and rank (" + error + output + ")");

    const std::array<std::string_view, 3> mirArguments = {
        "--dump-project-oop-mir", consumerProject, "-O0"};
    output.clear();
    error.clear();
    tests.expect(runDriver(mirArguments, output, error) == ExitCode::Success &&
            output.find("range-array-allocate type=builtin:int[,] element=builtin:int") !=
                std::string::npos && output.find("rank=2") != std::string::npos,
        "MIR schema publishes rank, element layout and rectangular operations (" + error + output + ")");

    const std::string debugObject = (consumer / "RangeConsumer.debug.obj").string();
    const std::array<std::string_view, 6> debugArguments = {
        "-g", "-O0", "--emit-project-oop-obj", consumerProject, "-o", debugObject};
    output.clear();
    error.clear();
    const bool debugEmitted = runDriver(debugArguments, output, error) == ExitCode::Success;
    const std::string debugBytes = debugEmitted ? readFile(debugObject) : std::string{};
    tests.expect(debugEmitted && debugBytes.find("builtin:int[,]" ) != std::string::npos &&
            debugBytes.find("Dimension0") != std::string::npos &&
            debugBytes.find("Stride1") != std::string::npos,
        "CodeView retains rectangular type, dimensions and strides (" + error + output + ")");

    const std::filesystem::path invalid = root / "invalid";
    writeProject(invalid, "InvalidRange", "StaticLibrary");
    writeFile(invalid / "Invalid.ns", R"NS(
public class Invalid
{
    public static int Read(int[,] value) { return value[0]; }
}
)NS");
    nativesharp::frontend::ProjectCompilation invalidCompilation;
    std::string invalidError;
    tests.expect(!invalidCompilation.compileProjectMethodBodies(
            invalid / "InvalidRange.nsproj", invalidError),
        "rank mismatch is rejected before HIR lowering");

    const std::filesystem::path bounds = root / "bounds";
    writeProject(bounds, "BoundsRange");
    writeFile(bounds / "Main.ns", R"NS(
public class Program
{
    public static int Main()
    {
        int[,] value = new int[2, 2];
        return value[0, 2];
    }
}
)NS");
    const std::string boundsProject = (bounds / "BoundsRange.nsproj").string();
    const std::string boundsExe = (bounds / "BoundsRange.exe").string();
    const std::array<std::string_view, 5> boundsArguments = {
        "-O0", "--emit-project-oop-exe", boundsProject, "-o", boundsExe};
    output.clear();
    error.clear();
    const bool boundsEmitted = runDriver(boundsArguments, output, error) == ExitCode::Success;
    tests.expect(boundsEmitted, "bounds fixture compiles (" + error + output + ")");
    if (boundsEmitted)
    {
        const std::string command = "cmd.exe /d /s /c \"\"" + boundsExe + "\"\"";
        tests.expect(std::system(command.c_str()) != 0,
            "each rectangular dimension is bounds checked at runtime");
    }

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
