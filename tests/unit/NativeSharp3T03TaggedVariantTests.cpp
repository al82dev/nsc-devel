#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryAbiReport.h"
#include "nativesharp/Interop/LibraryApiDiff.h"
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
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

void writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source,
    std::string_view properties = {},
    std::string_view items = {})
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language>" + std::string(properties) +
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />" +
        std::string(items) + "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
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

bool rejected(
    const std::filesystem::path& directory,
    std::string_view source,
    std::string_view expected,
    std::string& diagnostic)
{
    writeProject(directory, "RejectedVariant", source);
    const std::string project = (directory / "RejectedVariant.nsproj").string();
    const std::array<std::string_view, 3> arguments = {
        "--check-project-oop-hir", project, "-O0"};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    return result != ExitCode::Success && diagnostic.find(expected) != std::string::npos;
}
}

int main()
{
    using namespace nativesharp::interop;
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t03_tagged_variants";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    const std::filesystem::path direct = root / "direct";
    writeProject(direct, "TaggedVariants", R"NS(
namespace TaggedVariants
{
    public variant OptionalPointer { None, Some(int*) }
    public variant Recursive { End, Next(Recursive*) }
    public variant Result<T, E> { Ok(T), Error(E) }

    public class Program
    {
        public static int Main()
        {
            Result<int, int> result = Result<int, int>.Ok(40);
            int answer = 0;
            match (result)
            {
                Ok(value) => { answer = value + 2; },
                Error(error) => { return error; }
            }
            if (!Recursive.End().IsEnd() || Recursive.End().CaseTag() != 0) { return 2; }
            if (!OptionalPointer.None().IsNone() || OptionalPointer.None().CaseTag() != 0)
            {
                return 3;
            }
            return answer;
        }
    }
}
)NS");
    const std::string directProject = (direct / "TaggedVariants.nsproj").string();
    std::string output;
    std::string error;
    const std::array<std::string_view, 3> layoutArguments = {
        "--dump-project-layouts", directProject, "-O0"};
    const ExitCode layoutResult = runDriver(layoutArguments, output, error);
    tests.expect(
        layoutResult == ExitCode::Success &&
            output.find("OptionalPointer size=16 align=8") != std::string::npos &&
            output.find("Recursive size=16 align=8") != std::string::npos &&
            output.find("name=__nsc_variant_tag type=builtin:byte offset=0 size=1 align=1") !=
                std::string::npos &&
            output.find("name=__nsc_variant_payload") != std::string::npos &&
            output.find("offset=8 size=8 align=8") != std::string::npos,
        "Win64 variants use an explicit one-byte tag and aligned in-place payload; pointer "
        "variants do not acquire an unauthorized niche layout (" + error + output + ")");

    const std::string directExecutable = (direct / "TaggedVariants.exe").string();
    const std::array<std::string_view, 5> directArguments = {
        "--emit-project-oop-exe", directProject, "-O0", "-o", directExecutable};
    output.clear();
    error.clear();
    const ExitCode directResult = runDriver(directArguments, output, error);
    tests.expect(
        directResult == ExitCode::Success,
        "generic and indirect-recursive variants compile at O0 through the selected linker (" +
            error + output + ")");
    if (directResult == ExitCode::Success)
    {
        tests.expect(
            std::system(("\"" + directExecutable + "\"").c_str()) == 42,
            "tag construction, tests and generic payload extraction execute correctly");
    }

    const std::filesystem::path owning = root / "owning";
    writeProject(owning, "OwningVariant", R"NS(
public class Resource
{
    public static int Drops = 0;
    public int Value;
    public Resource(int value) { Value = value; }
    ~Resource() { Drops = Drops + 1; }
}

public variant Owner
{
    Empty,
    Value(owned Resource)
}

public class Program
{
    public static int Main()
    {
        {
            Owner owner = Owner.Value(new Resource(20));
            owner = Owner.Empty();
            if (Resource.Drops != 1) { return 10; }
            owner = Owner.Value(new Resource(22));
        }
        if (Resource.Drops != 2) { return 11; }
        return 42;
    }
}
)NS");
    const std::string owningProject = (owning / "OwningVariant.nsproj").string();
    const std::string owningExecutable = (owning / "OwningVariant.exe").string();
    const std::array<std::string_view, 5> owningArguments = {
        "--emit-project-oop-exe", owningProject, "-O0", "-o", owningExecutable};
    output.clear();
    error.clear();
    const ExitCode owningResult = runDriver(owningArguments, output, error);
    tests.expect(
        owningResult == ExitCode::Success,
        "a move-only owned payload compiles through the selected linker (" +
            error + output + ")");
    if (owningResult == ExitCode::Success)
    {
        tests.expect(
            std::system(("\"" + owningExecutable + "\"").c_str()) == 42,
            "changing the active case drops the displaced payload and scope exit drops the "
            "remaining payload exactly once");
    }

    std::string diagnostic;
    tests.expect(
        rejected(root / "empty", "variant Empty { }", "at least one case", diagnostic),
        "an empty closed domain is rejected (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(
        rejected(root / "duplicate", "variant Bad { Same, Same }", "duplicate variant case", diagnostic),
        "duplicate case identities are rejected (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(
        rejected(root / "recursive", "variant Loop { Next(Loop) }", "recursive", diagnostic),
        "a direct by-value recursive variant is rejected while pointer recursion remains legal (" +
            diagnostic + ")");
    tests.expect(
        rejected(root / "layout", "pack(1) variant Packed { Value(int) }",
            "cannot override the canonical tagged-variant layout", diagnostic),
        "explicit pack/align/size cannot corrupt canonical variant layout (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(
        rejected(root / "readonly", "readonly variant Frozen { Value(int) }",
            "readonly applies only to non-union struct", diagnostic),
        "readonly remains a struct-view modifier and cannot redefine variant construction (" +
            diagnostic + ")");
    diagnostic.clear();
    tests.expect(
        rejected(root / "borrowed", "borrowed variant View { Value(int) }",
            "variants own their active storage", diagnostic),
        "borrowed remains a struct-view declaration rather than a variant layout modifier (" +
            diagnostic + ")");
    diagnostic.clear();
    tests.expect(
        rejected(root / "borrowed-payload",
            "variant View { Value(borrowed string) }",
            "outside the NativeSharp 3.0 storage contract", diagnostic),
        "a direct borrowed payload is rejected with a final 3.0 contract rather than a stale "
        "future-stage dependency (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(
        rejected(root / "reserved", "variant Collision { CaseTag }",
            "collides with generated member", diagnostic),
        "case names cannot collide with the generated public surface (" + diagnostic + ")");

    const std::filesystem::path producer = root / "producer";
    writeProject(producer, "VariantApi", R"NS(
namespace VariantApi
{
    public variant Signal { Zulu, Alpha(int) }
    public variant Result<T, E> { Ok(T), Error(E) }
}
)NS", "<OutputType>StaticLibrary</OutputType>");
    const std::string producerProject = (producer / "VariantApi.nsproj").string();
    const std::string producerLibrary = (producer / "VariantApi.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "--emit-project-oop-lib", producerProject, "-O0", "-o", producerLibrary};
    output.clear();
    error.clear();
    const ExitCode producerResult = runDriver(producerArguments, output, error);
    tests.expect(
        producerResult == ExitCode::Success,
        "variant producer emits an O0 static library through NSLink (" + error + output + ")");

    const std::filesystem::path metadataPath = producer / "VariantApi.nsmeta";
    LibraryMetadata metadata;
    LibraryMetadataCodec codec;
    std::string metadataError;
    const bool metadataRead = codec.read(metadataPath, metadata, metadataError);
    const auto signal = std::ranges::find_if(
        metadata.types,
        [](const LibraryMetadataType& type)
        {
            return type.fullyQualifiedName == "VariantApi.Signal";
        });
    tests.expect(
        metadataRead && metadata.formatMinor >= 27 && signal != metadata.types.end() &&
            (signal->flags & kMetadataTypeVariant) != 0U && signal->variantCases.size() == 2U &&
            signal->variantCases[0].name == "Zulu" &&
            signal->variantCases[0].payloadTypeKey.empty() &&
            signal->variantCases[1].name == "Alpha" &&
            signal->variantCases[1].payloadTypeKey == "builtin:int",
        "NSMETA 1.27 preserves nominal variant identity, declaration order and payload types (" +
            metadataError + ")");

    std::ostringstream abiReport;
    LibraryAbiReporter::write(metadata, abiReport);
    tests.expect(
        abiReport.str().find("variant-case #0 name=Zulu") != std::string::npos &&
            abiReport.str().find("variant-case #1 name=Alpha payload=builtin:int") !=
                std::string::npos,
        "the ABI report exposes ordered variant cases and payload identities");

    LibraryMetadata reordered = metadata;
    const auto reorderedSignal = std::ranges::find_if(
        reordered.types,
        [](const LibraryMetadataType& type)
        {
            return type.fullyQualifiedName == "VariantApi.Signal";
        });
    if (reorderedSignal != reordered.types.end())
        std::ranges::reverse(reorderedSignal->variantCases);
    const LibraryApiDiffReport reorderedDiff = LibraryApiDiffer::compare(metadata, reordered);
    tests.expect(
        reorderedSignal != reordered.types.end() &&
            (!reorderedDiff.sourceCompatible || !reorderedDiff.abiCompatible),
        "API diff classifies CaseTag order changes instead of treating them as cosmetic");

    LibraryMetadata downlevel = metadata;
    downlevel.formatMinor = 26;
    std::string downlevelError;
    tests.expect(
        !codec.writeModel(downlevel, root / "variant-cannot-be-1.26.nsmeta", downlevelError) &&
            downlevelError.find("before 1.27") != std::string::npos,
        "a tagged-variant catalogue cannot be mislabeled as NSMETA 1.26 (" +
            downlevelError + ")");

    LibraryMetadata historical;
    historical.formatMinor = 26;
    historical.producerVersion = metadata.producerVersion;
    historical.moduleName = "Historical126";
    historical.targetTriple = metadata.targetTriple;
    historical.languageEdition = 3;
    const std::filesystem::path historicalPath = root / "historical-1.26.nsmeta";
    std::string historicalError;
    LibraryMetadata historicalRead;
    tests.expect(
        codec.writeModel(historical, historicalPath, historicalError) &&
            codec.read(historicalPath, historicalRead, historicalError) &&
            historicalRead.formatMinor == 26 &&
            codec.verifyCurrentCompiler(historicalRead, historicalError),
        "the 1.27 reader retains compatibility with a genuine variant-free NSMETA 1.26 model (" +
            historicalError + ")");

    LibraryMetadata malformed = metadata;
    const auto malformedSignal = std::ranges::find_if(
        malformed.types,
        [](const LibraryMetadataType& type)
        {
            return type.fullyQualifiedName == "VariantApi.Signal";
        });
    if (malformedSignal != malformed.types.end())
        malformedSignal->flags &= static_cast<std::uint16_t>(~kMetadataTypeVariant);
    std::string malformedError;
    tests.expect(
        malformedSignal != malformed.types.end() &&
            !codec.verifyCurrentCompiler(malformed, malformedError) &&
            malformedError.find("without the variant flag") != std::string::npos,
        "NSMETA rejects a detached variant case catalogue (" + malformedError + ")");

    const std::filesystem::path consumer = root / "consumer";
    const std::string libraryItem =
        "<LibraryReference Include=\"" + (producer / "VariantApi.lib").generic_string() +
        "\" Metadata=\"" + metadataPath.generic_string() + "\" />";
    writeProject(consumer, "VariantConsumer", R"NS(
using VariantApi;
class Program
{
    public static int Main()
    {
        Signal signal = Signal.Zulu();
        if (!signal.IsZulu() || signal.CaseTag() != 0) { return 1; }
        Signal alpha = Signal.Alpha(42);
        if (!alpha.IsAlpha() || alpha.CaseTag() != 1) { return 2; }
        match (alpha)
        {
            Alpha(value) => { return value; },
            Zulu => { return 4; }
        }
    }
}
)NS", {}, libraryItem);
    const std::string consumerProject = (consumer / "VariantConsumer.nsproj").string();
    const std::string consumerExecutable = (consumer / "VariantConsumer.exe").string();
    const std::array<std::string_view, 5> consumerArguments = {
        "--emit-project-oop-exe", consumerProject, "-O0", "-o", consumerExecutable};
    output.clear();
    error.clear();
    const ExitCode consumerResult = runDriver(consumerArguments, output, error);
    tests.expect(
        consumerResult == ExitCode::Success,
        "LibraryReference rehydrates ordered unit and payload variants (" +
            error + output + ")");
    if (consumerResult == ExitCode::Success)
    {
        tests.expect(
            std::system(("\"" + consumerExecutable + "\"").c_str()) == 42,
            "variant identity and CaseTag order survive the NSMETA producer/consumer boundary");
    }

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
