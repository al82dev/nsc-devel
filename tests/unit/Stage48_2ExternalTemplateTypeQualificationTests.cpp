#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::interop::LibraryMetadata;
using nativesharp::interop::LibraryMetadataCodec;
using nativesharp::tests::TestContext;

std::string utf8Path(const std::filesystem::path& path)
{
    const std::u8string value = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

ExitCode emitLibrary(
    const std::filesystem::path& project,
    const std::filesystem::path& destination,
    std::string& error)
{
    Driver driver;
    std::ostringstream output;
    std::ostringstream diagnostics;
    const std::string projectText = utf8Path(project);
    const std::string destinationText = utf8Path(destination);
    const std::array<std::string_view, 4> arguments = {
        "--emit-project-oop-lib", projectText, "-o", destinationText};
    const ExitCode result = driver.run(arguments, output, diagnostics);
    error = diagnostics.str();
    return result;
}

bool contains(std::span<const std::string> values, std::string_view expected)
{
    return std::ranges::find(values, expected) != values.end();
}

void testExternalTypesInReusableTemplate(
    TestContext& tests,
    const std::filesystem::path& root)
{
    const auto external = root / "External";
    const auto producer = root / "Producer";
    const auto consumer = root / "Consumer";

    writeFile(external / "External.ns", R"NS(
namespace Foreign.Memory
{
    public interface IAllocator
    {
        public int Marker();
    }
}
)NS");
    writeFile(external / "ForeignTypes.nsproj",
        R"XML(<Project><PropertyGroup><Name>ForeignTypes</Name>
        <OutputType>StaticLibrary</OutputType></PropertyGroup>
        <ItemGroup><Compile Include="**/*.ns" /></ItemGroup></Project>)XML");

    writeFile(producer / "Collections.ns", R"NS(
using Foreign.Memory;

namespace Collections.Core
{
    public class ArrayOps
    {
        public static void FreeArray<T>(IAllocator allocator, T* items, long capacity)
        {
        }

        public static T* TryAllocateArray<T>(
            IAllocator allocator,
            long count,
            out long capacity)
        {
            capacity = count;
            return null;
        }
    }
}
)NS");
    writeFile(producer / "CollectionTemplates.nsproj",
        R"XML(<Project><PropertyGroup><Name>CollectionTemplates</Name>
        <OutputType>StaticLibrary</OutputType></PropertyGroup><ItemGroup>
        <Compile Include="**/*.ns" />
        <LibraryReference Include="../External/ForeignTypes.lib" />
        </ItemGroup></Project>)XML");

    std::string error;
    const auto externalLibrary = external / "ForeignTypes.lib";
    const ExitCode externalResult =
        emitLibrary(external / "ForeignTypes.nsproj", externalLibrary, error);
    tests.expect(externalResult == ExitCode::Success,
        "O0 external type provider emits NSMETA and archive (" + error + ")");
    if (externalResult != ExitCode::Success) return;

    error.clear();
    const auto producerLibrary = producer / "CollectionTemplates.lib";
    const ExitCode producerResult =
        emitLibrary(producer / "CollectionTemplates.nsproj", producerLibrary, error);
    tests.expect(producerResult == ExitCode::Success,
        "O0 producer emits reusable templates that reference external nominal types (" + error + ")");
    if (producerResult != ExitCode::Success) return;

    LibraryMetadata metadata;
    LibraryMetadataCodec codec;
    error.clear();
    tests.expect(codec.read(producer / "CollectionTemplates.nsmeta", metadata, error),
        "producer NSMETA remains readable after canonical template source emission (" + error + ")");
    const auto freeArray = std::ranges::find_if(metadata.genericTemplates, [](const auto& generic) {
        return generic.name == "FreeArray";
    });
    const auto tryAllocate = std::ranges::find_if(metadata.genericTemplates, [](const auto& generic) {
        return generic.name == "TryAllocateArray";
    });
    tests.expect(freeArray != metadata.genericTemplates.end() &&
            freeArray->sourceText.find(
                "ForeignTypes::Foreign.Memory.IAllocator") != std::string::npos,
        "public template parameter keeps the external module and namespace; source=" +
            (freeArray == metadata.genericTemplates.end()
                ? std::string("<missing>") : freeArray->sourceText));
    tests.expect(tryAllocate != metadata.genericTemplates.end() &&
            tryAllocate->sourceText.find(
                "ForeignTypes::Foreign.Memory.IAllocator") != std::string::npos,
        "multiline public template signature keeps the external module and namespace; source=" +
            (tryAllocate == metadata.genericTemplates.end()
                ? std::string("<missing>") : tryAllocate->sourceText));
    tests.expect(contains(
            metadata.externalTypeKeys,
            "type:ForeignTypes::Foreign.Memory.IAllocator"),
        "NSMETA external-type closure includes nominal types used only by reusable templates; count=" +
            std::to_string(metadata.externalTypeKeys.size()) +
            (metadata.externalTypeKeys.empty()
                ? std::string{}
                : std::string(" first=") + metadata.externalTypeKeys.front()));

    writeFile(consumer / "Consumer.ns", R"NS(
using Collections.Core;

public class ConsumerMarker
{
    public static int Value()
    {
        ArrayOps.FreeArray<int>(null, null, 0);
        return 42;
    }
}
)NS");
    writeFile(consumer / "TemplateConsumer.nsproj",
        R"XML(<Project><PropertyGroup><Name>TemplateConsumer</Name>
        <OutputType>StaticLibrary</OutputType></PropertyGroup><ItemGroup>
        <Compile Include="**/*.ns" />
        <LibraryReference Include="../External/ForeignTypes.lib" />
        <LibraryReference Include="../Producer/CollectionTemplates.lib" />
        </ItemGroup></Project>)XML");
    error.clear();
    const ExitCode consumerResult =
        emitLibrary(
            consumer / "TemplateConsumer.nsproj",
            consumer / "TemplateConsumer.lib",
            error);
    tests.expect(consumerResult == ExitCode::Success,
        "O0 consumer parses external template types using only LibraryReference artifacts (" +
            error + ")");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        "nativesharp_stage48_2_external_template_type_qualification";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    testExternalTypesInReusableTemplate(tests, root);

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
