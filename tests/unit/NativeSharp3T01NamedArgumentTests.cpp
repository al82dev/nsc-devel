#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <span>
#include <string>
#include <string_view>

namespace
{
using nativesharp::driver::Driver;
using nativesharp::ExitCode;
using nativesharp::interop::LibraryMetadata;
using nativesharp::interop::LibraryMetadataCodec;
using nativesharp::interop::LibraryMetadataMember;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return result;
}

void writeConsumerProject(
    const std::filesystem::path& directory,
    const std::filesystem::path& library,
    const std::filesystem::path& metadata,
    std::string_view source)
{
    writeFile(
        directory / "Consumer.nsproj",
        "<Project><PropertyGroup><Name>Consumer</Name><Language>3</Language>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
        "<LibraryReference Include=\"" + library.generic_string() + "\" Metadata=\"" +
        metadata.generic_string() + "\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
}

bool compileRejected(
    const std::filesystem::path& directory,
    const std::filesystem::path& library,
    const std::filesystem::path& metadata,
    std::string_view source,
    std::string& diagnostic)
{
    writeConsumerProject(directory, library, metadata, source);
    const std::string project = (directory / "Consumer.nsproj").string();
    const std::array<std::string_view, 3> arguments = {
        "--check-project-oop-hir", project, "-O0"};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    return result != ExitCode::Success;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t01_named_arguments";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    const std::filesystem::path producer = root / "producer";
    writeFile(
        producer / "NamedApi.nsproj",
        "<Project><PropertyGroup><Name>NamedApi</Name><Language>3</Language>"
        "<OutputType>StaticLibrary</OutputType></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(producer / "Api.ns", R"NS(
namespace NamedApi
{
    public class Codec
    {
        public static int Encode(int first, int second = 2, int third = 3)
        {
            return first * 100 + second * 10 + third;
        }

        public static int Pick(int value) { return 42; }
        public static int Pick(int value, int optional = 0) { return 0; }

        public static int Generic<T>(T first, T second) { return 42; }
    }
}
)NS");

    const std::string producerProject = (producer / "NamedApi.nsproj").string();
    const std::string producerLibrary = (producer / "NamedApi.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "--emit-project-oop-lib", producerProject, "-O0", "-o", producerLibrary};
    std::string output;
    std::string error;
    const ExitCode producerResult = runDriver(producerArguments, output, error);
    tests.expect(
        producerResult == ExitCode::Success,
        "named-argument producer emits an O0 library through NSLink (" + error + output + ")");

    const std::filesystem::path metadataPath = producer / "NamedApi.nsmeta";
    LibraryMetadata metadata;
    std::string metadataError;
    const bool metadataRead =
        LibraryMetadataCodec{}.read(metadataPath, metadata, metadataError);
    const auto encode = std::ranges::find_if(
        metadata.members,
        [](const LibraryMetadataMember& member)
        {
            return member.name == "Encode" && member.parameters.size() == 3;
        });
    tests.expect(
        metadataRead && encode != metadata.members.end() &&
            encode->parameters[0].name == "first" &&
            encode->parameters[1].name == "second" &&
            encode->parameters[2].name == "third",
        "NSMETA keeps public parameter names and declaration ordinals (" + metadataError + ")");

    const std::filesystem::path consumer = root / "consumer";
    writeConsumerProject(
        consumer, producer / "NamedApi.lib", metadataPath, R"NS(
using NamedApi;

class Program
{
    private static int trace = 0;

    private static int Mark(int value)
    {
        trace = trace * 10 + value;
        return value;
    }

    public static int Main()
    {
        int positional = Codec.Encode(1, 2, 3);

        trace = 0;
        int reordered = Codec.Encode(
            third: Mark(3), first: Mark(1), second: Mark(2));
        if (reordered != positional || trace != 312) { return 1; }

        if (Codec.Encode(first: 1) != 123) { return 2; }
        if (Codec.Encode(1, third: 3, second: 2) != positional) { return 3; }
        if (Codec.Pick(value: 7) != 42) { return 4; }
        if (Codec.Generic<int>(second: 22, first: 20) != 42) { return 5; }
        return 42;
    }
}
)NS");
    const std::string consumerProject = (consumer / "Consumer.nsproj").string();
    const std::string consumerExecutable = (consumer / "Consumer.exe").string();
    const std::array<std::string_view, 5> consumerArguments = {
        "--emit-project-oop-exe", consumerProject, "-O0", "-o", consumerExecutable};
    output.clear();
    error.clear();
    const ExitCode consumerResult = runDriver(consumerArguments, output, error);
    tests.expect(
        consumerResult == ExitCode::Success,
        "precompiled consumer binds named arguments after overload formation (" +
            error + output + ")");
    if (consumerResult == ExitCode::Success)
    {
        const std::string command = "\"" + consumerExecutable + "\"";
        tests.expect(
            std::system(command.c_str()) == 42,
            "O0 preserves source evaluation order while the callee receives ABI parameter order");
    }

    std::string diagnostic;
    const bool duplicateRejected = compileRejected(
        root / "duplicate", producer / "NamedApi.lib", metadataPath,
        R"NS(using NamedApi; class Program { public static int Main() { return Codec.Encode(first: 1, first: 2); } })NS",
        diagnostic);
    tests.expect(
        duplicateRejected && diagnostic.find("no overload") != std::string::npos,
        "duplicate named arguments are rejected by overload binding (" + diagnostic + ")");

    diagnostic.clear();
    const bool unknownRejected = compileRejected(
        root / "unknown", producer / "NamedApi.lib", metadataPath,
        R"NS(using NamedApi; class Program { public static int Main() { return Codec.Encode(renamed: 1); } })NS",
        diagnostic);
    tests.expect(
        unknownRejected && diagnostic.find("no overload") != std::string::npos,
        "unknown or renamed public parameter names are rejected across NSMETA (" + diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
