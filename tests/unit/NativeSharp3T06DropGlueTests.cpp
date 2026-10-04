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
#include <algorithm>

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

ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    nativesharp::driver::Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return result;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t06_drop_glue";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    const std::filesystem::path project = root / "DropGlue.nsproj";
    writeFile(project,
        "<Project><PropertyGroup><Name>DropGlue</Name><Language>3</Language>"
        "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
        "<Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
public class Resource
{
    public static int Drops = 0;
    public int Value;
    public Resource(int value) { Value = value; }
    ~Resource() { Drops = Drops + 1; }
}

public variant Owner<T>
{
    Empty,
    Value(owned Resource)
}

public class Program
{
    public static int Main()
    {
        {
            Owner<int>[2] items = {
                Owner<int>.Value(new Resource(20)),
                Owner<int>.Value(new Resource(22))
            };
        }
        if (Resource.Drops != 2) { return 10; }
        return 42;
    }
}
)NS");

    const std::string projectText = project.string();
    const std::string executableText = (root / "DropGlue.exe").string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", projectText, "-O0", "-o", executableText};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    bool executesExactlyOnce = false;
    if (result == ExitCode::Success)
        executesExactlyOnce = std::system(("\"" + executableText + "\"").c_str()) == 42;
    tests.expect(
        result == ExitCode::Success && executesExactlyOnce,
        "generic variants inside fixed arrays destroy each active owned payload exactly once (" +
            error + output + ")");

    const std::filesystem::path producer = root / "producer";
    const std::filesystem::path producerProject = producer / "DropApi.nsproj";
    const std::filesystem::path producerLibrary = producer / "DropApi.lib";
    writeFile(producerProject,
        "<Project><PropertyGroup><Name>DropApi</Name><Language>3</Language>"
        "<OutputType>StaticLibrary</OutputType></PropertyGroup><ItemGroup>"
        "<Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(producer / "Api.ns", R"NS(
namespace DropApi
{
    public class Resource
    {
        public static int Drops = 0;
        public int Value;
        public Resource(int value) { Value = value; }
        ~Resource() { Drops = Drops + 1; }
    }
    public struct Box
    {
        public owned Resource Value;
        public Box(owned Resource value) { Value = move(value); }
    }
    public class Factory
    {
        public static Box Make() { return new Box(new Resource(42)); }
        public static int DropCount() { return Resource.Drops; }
    }
}
)NS");
    const std::string producerProjectText = producerProject.string();
    const std::string producerLibraryText = producerLibrary.string();
    const std::array<std::string_view, 5> producerArguments = {
        "--emit-project-oop-lib", producerProjectText, "-O0", "-o", producerLibraryText};
    output.clear();
    error.clear();
    const ExitCode producerResult = runDriver(producerArguments, output, error);

    nativesharp::interop::LibraryMetadata metadata;
    std::string metadataError;
    const bool metadataRead = producerResult == ExitCode::Success &&
        nativesharp::interop::LibraryMetadataCodec{}.read(
            producer / "DropApi.nsmeta", metadata, metadataError);
    const auto box = std::ranges::find_if(metadata.types, [](const auto& type)
    {
        return type.fullyQualifiedName == "DropApi.Box";
    });
    tests.expect(
        metadataRead && metadata.formatMinor == nativesharp::interop::kMetadataFormatMinor &&
            box != metadata.types.end() &&
            !box->dropGlueEntryName.empty(),
        "NSMETA 1.36 publishes the producer-owned value drop-glue identity (" +
            metadataError + error + output + ")");
    if (metadataRead && box != metadata.types.end())
    {
        nativesharp::interop::LibraryMetadata malformed = metadata;
        const auto malformedBox = std::ranges::find_if(malformed.types, [](const auto& type)
        {
            return type.fullyQualifiedName == "DropApi.Box";
        });
        if (malformedBox != malformed.types.end()) malformedBox->dropGlueEntryName.clear();
        std::string malformedError;
        const bool malformedWritten =
            nativesharp::interop::LibraryMetadataCodec{}.writeModel(
                malformed, producer / "malformed.nsmeta", malformedError);
        nativesharp::interop::LibraryMetadata malformedRead;
        const bool malformedReadable = malformedWritten &&
            nativesharp::interop::LibraryMetadataCodec{}.read(
                producer / "malformed.nsmeta", malformedRead, malformedError);
        tests.expect(
            malformedReadable &&
                !nativesharp::interop::LibraryMetadataCodec{}.verifyCurrentCompiler(
                    malformedRead, malformedError) &&
                malformedError.find("no producer drop-glue entry") != std::string::npos,
            "NSMETA 1.36 rejects an owning value whose drop-glue identity was removed (written=" +
                std::string(malformedWritten ? "true" : "false") + ", " + malformedError + ")");

        nativesharp::interop::LibraryMetadata previous = metadata;
        previous.formatMinor = 35;
        for (auto& type : previous.types) type.dropGlueEntryName.clear();
        std::string previousError;
        const bool previousWritten =
            nativesharp::interop::LibraryMetadataCodec{}.writeModel(
                previous, producer / "previous-1.35.nsmeta", previousError);
        nativesharp::interop::LibraryMetadata previousRead;
        const bool previousReadable = previousWritten &&
            nativesharp::interop::LibraryMetadataCodec{}.read(
                producer / "previous-1.35.nsmeta", previousRead, previousError);
        tests.expect(
            previousReadable && previousRead.formatMinor == 35,
            "the current reader retains the NSMETA 1.35 compatibility window (" +
                previousError + ")");
    }
    else
    {
        tests.expect(false, "malformed NSMETA drop-glue fixture requires producer metadata");
        tests.expect(false, "NSMETA 1.35 compatibility fixture requires producer metadata");
    }

    const std::filesystem::path consumer = root / "consumer";
    const std::filesystem::path consumerProject = consumer / "DropConsumer.nsproj";
    writeFile(consumerProject,
        "<Project><PropertyGroup><Name>DropConsumer</Name><Language>3</Language>"
        "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
        "<Compile Include=\"**/*.ns\" /><LibraryReference Include=\"" +
        producerLibrary.generic_string() + "\" /></ItemGroup></Project>");
    writeFile(consumer / "Main.ns", R"NS(
using DropApi;
public class Program
{
    public static int Main()
    {
        { Box value = Factory.Make(); }
        if (Factory.DropCount() != 1) { return 11; }
        return 42;
    }
}
)NS");
    const std::string consumerProjectText = consumerProject.string();
    const std::string consumerExecutableText = (consumer / "DropConsumer.exe").string();
    const std::array<std::string_view, 5> consumerArguments = {
        "--emit-project-oop-exe", consumerProjectText, "-O0", "-o", consumerExecutableText};
    output.clear();
    error.clear();
    const ExitCode consumerResult = runDriver(consumerArguments, output, error);
    bool crossModuleExactlyOnce = false;
    if (consumerResult == ExitCode::Success)
        crossModuleExactlyOnce =
            std::system(("\"" + consumerExecutableText + "\"").c_str()) == 42;
    tests.expect(
        consumerResult == ExitCode::Success && crossModuleExactlyOnce,
        "an imported value delegates destruction to the producer thunk exactly once (" +
            error + output + ")");

    return tests.resultKeeping(root);
}
