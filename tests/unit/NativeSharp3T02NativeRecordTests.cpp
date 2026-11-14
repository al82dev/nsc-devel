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
using nativesharp::interop::LibraryMetadataType;
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
    std::string_view extraItems = {})
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" />" + std::string(extraItems) +
        "</ItemGroup></Project>");
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
    writeProject(directory, "RejectedRecord", source);
    const std::string project = (directory / "RejectedRecord.nsproj").string();
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
    TestContext tests;
    using namespace nativesharp::interop;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t02_native_records";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    writeProject(root / "positive", "NativeRecords", R"NS(
namespace NativeRecords
{
    public record struct Empty derive(Equality, Hash, Print, Clone) { }
    public struct EmptyStruct { }

    public record struct Pair derive(Equality, Hash, Print, Clone)
    {
        public int X;
        public int Y;
    }

    public record struct HashOnly derive(Hash)
    {
        public int Value;
    }

    public record struct Box<T> derive(Clone)
    {
        public T Value;
    }

    public record struct UserPrint
    {
        public string ToString() { return "user"; }
    }

    public class Program
    {
        public static int Main()
        {
            Empty empty;
            if (!(empty == empty) || !empty.Equals(empty)) { return 1; }
            if (empty.GetHashCode() != empty.GetHashCode()) { return 2; }
            if (empty.ToString() != "Empty") { return 3; }
            Empty emptyClone = empty.Clone();
            if (!(emptyClone == empty)) { return 4; }
            if (sizeof(Empty) != sizeof(EmptyStruct)) { return 5; }

            Pair left = { 20, 22 };
            Pair right = left.Clone();
            if (left != right || !left.Equals(right)) { return 6; }
            if (left.GetHashCode() != right.GetHashCode()) { return 7; }
            if (left.ToString() != "Pair") { return 8; }

            HashOnly partial = { 42 };
            if (partial.GetHashCode() != partial.GetHashCode()) { return 9; }
            Box<int> generic = { 42 };
            if (generic.Clone().Value != 42) { return 10; }
            UserPrint custom;
            if (custom.ToString() != "user") { return 11; }
            return 42;
        }
    }
}
)NS");

    const std::string positiveProject =
        (root / "positive" / "NativeRecords.nsproj").string();
    const std::string positiveExecutable =
        (root / "positive" / "NativeRecords.exe").string();
    std::string output;
    std::string error;
    const std::array<std::string_view, 5> emitArguments = {
        "--emit-project-oop-exe", positiveProject, "-O0", "-o", positiveExecutable};
    const ExitCode emitted = runDriver(emitArguments, output, error);
    tests.expect(
        emitted == ExitCode::Success,
        "O0 emits empty, partial, generic and user-shadowed native records (" +
            error + output + ")");
    if (emitted == ExitCode::Success)
    {
        tests.expect(
            std::system(("\"" + positiveExecutable + "\"").c_str()) == 42,
            "native records execute with struct layout and static derived operations");
    }

    const std::array<std::string_view, 3> hirArguments = {
        "--dump-project-oop-hir", positiveProject, "-O0"};
    output.clear();
    error.clear();
    const ExitCode dumped = runDriver(hirArguments, output, error);
    tests.expect(
        dumped == ExitCode::Success &&
            output.find("aggregate-equals") != std::string::npos &&
            output.find("aggregate-hash") != std::string::npos &&
            output.find("struct-clone") == std::string::npos,
        "record derives expand to aggregate operations/value copies without runtime identity (" +
            error + output + ")");

    std::string diagnostic;
    tests.expect(
        rejected(root / "nested_equality", R"NS(
record struct Inner { public int Value; }
record struct Outer derive(Equality) { public Inner Value; }
class Program { public static bool Same(Outer left, Outer right) { return left == right; } }
)NS", "does not derive Equality", diagnostic),
        "outer Equality rejects a nested record that did not opt in (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "owned_clone", R"NS(
record struct Owner derive(Clone) { public owned string Text; }
class Program { public static Owner Copy(Owner value) { return value.Clone(); } }
)NS", "would create two owners", diagnostic),
        "Clone rejects owned storage instead of synthesizing a double owner (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(
        rejected(root / "destructor_clone", R"NS(
record struct Resource derive(Clone) { ~Resource() { } }
class Program { public static Resource Copy(Resource value) { return value.Clone(); } }
)NS", "generated Clone is a plain value copy and would duplicate a non-trivial lifetime", diagnostic),
        "Clone rejects a destructor-bearing record instead of duplicating its lifetime (" +
            diagnostic + ")");

    const std::filesystem::path producer = root / "producer";
    writeFile(
        producer / "RecordApi.nsproj",
        "<Project><PropertyGroup><Name>RecordApi</Name><Language>3</Language>"
        "<OutputType>StaticLibrary</OutputType></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(producer / "Api.ns", R"NS(
namespace RecordApi
{
    public record struct Token derive(Equality, Hash, Print, Clone)
    {
        public int Left;
        public int Right;
    }
}
)NS");
    const std::string producerProject = (producer / "RecordApi.nsproj").string();
    const std::string producerLibrary = (producer / "RecordApi.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "--emit-project-oop-lib", producerProject, "-O0", "-o", producerLibrary};
    output.clear();
    error.clear();
    const ExitCode producerResult = runDriver(producerArguments, output, error);
    tests.expect(
        producerResult == ExitCode::Success,
        "record producer emits an O0 static library through NSLink (" + error + output + ")");

    const std::filesystem::path metadataPath = producer / "RecordApi.nsmeta";
    LibraryMetadata metadata;
    std::string metadataError;
    LibraryMetadataCodec metadataCodec;
    const bool metadataRead = metadataCodec.read(metadataPath, metadata, metadataError);
    const auto token = std::ranges::find_if(
        metadata.types,
        [](const LibraryMetadataType& type)
        {
            return type.fullyQualifiedName == "RecordApi.Token";
        });
    tests.expect(
        metadataRead && metadata.formatMinor == kMetadataFormatMinor &&
            token != metadata.types.end() &&
            (token->flags & kMetadataTypeRecord) != 0U &&
            (token->flags & kMetadataTypeRecordDeriveMask) == kMetadataTypeRecordDeriveMask,
        "NSMETA 1.26 publishes record identity and every explicit derive (" +
            metadataError + ")");

    LibraryMetadata relabeled = metadata;
    relabeled.formatMinor = 0;
    relabeled.formatMajor = 0;
    relabeled.payloadDecodedByCurrentReader = false;
    std::string relabeledError;
    tests.expect(metadataRead && metadataCodec.verifyCurrentCompiler(relabeled, relabeledError),
        "record content is verified independently of header labels (" + relabeledError + ")");

    LibraryMetadata invalidDelegate = relabeled;
    LibraryMetadataType delegateType;
    delegateType.kind = LibraryMetadataTypeKind::Delegate;
    delegateType.identityKey = "RecordApi.BadDelegate";
    delegateType.fullyQualifiedName = "RecordApi.BadDelegate";
    delegateType.referenceSize = 1;
    delegateType.referenceAlignment = invalidDelegate.pointerAlignment;
    delegateType.delegateReturnTypeKey = "builtin:int";
    invalidDelegate.types.push_back(delegateType);
    std::string invalidDelegateError;
    tests.expect(!metadataCodec.verifyCurrentCompiler(invalidDelegate, invalidDelegateError) &&
            invalidDelegateError.find("reference ABI") != std::string::npos,
        "delegate reference ABI is checked independently of header labels (" + invalidDelegateError + ")");
    invalidDelegate.types.back().referenceSize = invalidDelegate.pointerSize;
    invalidDelegate.types.back().delegateReturnTypeKey.clear();
    invalidDelegateError.clear();
    tests.expect(!metadataCodec.verifyCurrentCompiler(invalidDelegate, invalidDelegateError) &&
            invalidDelegateError.find("callable signature") != std::string::npos,
        "delegate signature is checked independently of header labels (" + invalidDelegateError + ")");

    LibraryMetadata invalidGeneric = relabeled;
    LibraryMetadataGenericTemplate generic;
    generic.canonicalKey = "RecordApi::BadGeneric";
    invalidGeneric.genericTemplates.push_back(generic);
    std::string invalidGenericError;
    tests.expect(!metadataCodec.verifyCurrentCompiler(invalidGeneric, invalidGenericError) &&
            invalidGenericError.find("materializable declaration") != std::string::npos,
        "generic template content is checked independently of header labels (" + invalidGenericError + ")");

    LibraryMetadata invalidDependency = relabeled;
    LibraryMetadataBinaryDependency dependency;
    dependency.moduleName = "External";
    invalidDependency.binaryDependencies.push_back(dependency);
    std::string invalidDependencyError;
    tests.expect(!metadataCodec.verifyCurrentCompiler(invalidDependency, invalidDependencyError) &&
            invalidDependencyError.find("binary dependency") != std::string::npos,
        "binary dependency content is checked independently of header labels (" + invalidDependencyError + ")");

    LibraryMetadata invalidExternalType = relabeled;
    invalidExternalType.externalTypeKeys.push_back("type:RecordApi::Missing");
    std::string invalidExternalTypeError;
    tests.expect(!metadataCodec.verifyCurrentCompiler(invalidExternalType, invalidExternalTypeError) &&
            invalidExternalTypeError.find("external dependency") != std::string::npos,
        "external type contracts are checked independently of header labels (" + invalidExternalTypeError + ")");

    LibraryMetadata invalidMember = relabeled;
    LibraryMetadataMember invalidMethod;
    invalidMethod.kind = 2U;
    invalidMethod.declaringTypeKey = "type:RecordApi::Token";
    invalidMethod.name = "Bad";
    invalidMethod.signatureKey = "Bad()";
    invalidMethod.declaredTypeKey = "builtin:int";
    invalidMethod.flags = kMetadataMemberReadonlyReferenceReturn;
    invalidMember.members.push_back(invalidMethod);
    std::string invalidMemberError;
    const bool invalidMemberAccepted = metadataCodec.verifyCurrentCompiler(invalidMember, invalidMemberError);
    tests.expect(!invalidMemberAccepted &&
            invalidMemberError.find("ref readonly without ref-return") != std::string::npos,
        "member contracts are checked independently of header labels (" + invalidMemberError + ")");

    LibraryMetadata malformed = metadata;
    const auto malformedToken = std::ranges::find_if(
        malformed.types,
        [](const LibraryMetadataType& type)
        {
            return type.fullyQualifiedName == "RecordApi.Token";
        });
    if (malformedToken != malformed.types.end())
        malformedToken->flags &= static_cast<std::uint16_t>(~kMetadataTypeRecord);
    std::string malformedError;
    tests.expect(
        malformedToken != malformed.types.end() &&
            !metadataCodec.verifyCurrentCompiler(malformed, malformedError) &&
            malformedError.find("without the record flag") != std::string::npos,
        "NSMETA rejects derive bits detached from record identity (" + malformedError + ")");

    const std::filesystem::path consumer = root / "consumer";
    const std::string libraryItem =
        "<LibraryReference Include=\"" + (producer / "RecordApi.lib").generic_string() +
        "\" Metadata=\"" + metadataPath.generic_string() + "\" />";
    writeProject(consumer, "RecordConsumer", R"NS(
using RecordApi;
class Program
{
    public static int Main()
    {
        Token value = { 20, 22 };
        Token copy = value.Clone();
        if (value != copy || !value.Equals(copy)) { return 1; }
        if (value.GetHashCode() != copy.GetHashCode()) { return 2; }
        if (value.ToString() != "Token") { return 3; }
        return copy.Left + copy.Right;
    }
}
)NS", libraryItem);
    const std::string consumerProject = (consumer / "RecordConsumer.nsproj").string();
    const std::string consumerExecutable = (consumer / "RecordConsumer.exe").string();
    const std::array<std::string_view, 5> consumerArguments = {
        "--emit-project-oop-exe", consumerProject, "-O0", "-o", consumerExecutable};
    output.clear();
    error.clear();
    const ExitCode consumerResult = runDriver(consumerArguments, output, error);
    tests.expect(
        consumerResult == ExitCode::Success,
        "precompiled consumer reconstructs record derives without managed helpers (" +
            error + output + ")");
    if (consumerResult == ExitCode::Success)
    {
        tests.expect(
            std::system(("\"" + consumerExecutable + "\"").c_str()) == 42,
            "record derive policy survives the NSMETA producer/consumer boundary");
    }

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
