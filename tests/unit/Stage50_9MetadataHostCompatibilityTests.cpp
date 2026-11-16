#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"
#include "../support/MetadataHeaderControl.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::interop::LibraryMetadata;
using nativesharp::interop::LibraryMetadataCodec;
using nativesharp::interop::LibraryMetadataMember;
using nativesharp::interop::LibraryMetadataOwnershipKind;
using nativesharp::interop::LibraryMetadataParameterPassKind;
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
    return std::string(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
}

const LibraryMetadataMember* memberNamed(
    const LibraryMetadata& metadata,
    std::string_view name)
{
    const auto found = std::find_if(
        metadata.members.begin(), metadata.members.end(),
        [&](const LibraryMetadataMember& member) { return member.name == name; });
    return found == metadata.members.end() ? nullptr : &*found;
}

void writeProject(
    const std::filesystem::path& directory,
    std::string_view projectName,
    std::string_view source)
{
    writeFile(
        directory / (std::string(projectName) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(projectName) +
            "</Name><OutputType>StaticLibrary</OutputType></PropertyGroup>"
            "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Api.ns", source);
}

ExitCode runDriver(
    std::span<const std::string_view> args,
    std::string& output,
    std::string& error)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode code = driver.run(args, out, err);
    output = out.str();
    error = err.str();
    return code;
}

ExitCode emitMetadata(
    const std::filesystem::path& project,
    const std::filesystem::path& destination,
    std::string& output,
    std::string& error)
{
    const std::string projectText = project.string();
    const std::string destinationText = destination.string();
    const std::array<std::string_view, 4> args = {
        "--emit-project-library-metadata", projectText, "-o", destinationText};
    return runDriver(args, output, error);
}

ExitCode emitHeader(
    const std::filesystem::path& project,
    const std::filesystem::path& destination,
    std::string& output,
    std::string& error)
{
    const std::string projectText = project.string();
    const std::string destinationText = destination.string();
    const std::array<std::string_view, 4> args = {
        "--emit-project-host-header", projectText, "-o", destinationText};
    return runDriver(args, output, error);
}

const LibraryMetadataMember* findMember(const LibraryMetadata& metadata, std::string_view name)
{
    const auto found = std::find_if(metadata.members.begin(), metadata.members.end(), [name](const auto& member)
    {
        return member.name == name;
    });
    return found == metadata.members.end() ? nullptr : &*found;
}

const char* metadataSource()
{
    return R"NS(
namespace Stage50_9
{
    public class Resource
    {
        public int Value;
        public Resource(int value) { Value = value; }
        ~Resource() { Value = 0; }
    }

    public class Api
    {
        public static Resource Plain(Resource value)
        {
            return value;
        }

        public static owned Resource Take(owned Resource value)
        {
            return move(value);
        }

        public static borrowed Resource View(borrowed Resource value)
        {
            return value;
        }

        public static Resource* Raw(Resource* value)
        {
            return value;
        }
    }
}
)NS";
}

void testOwnershipRoundTrip(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "OwnershipMetadata";
    writeProject(directory, "OwnershipMetadata", metadataSource());

    const auto outputPath = directory / "OwnershipMetadata.nsmeta";
    std::string output;
    std::string error;
    tests.expect(
        emitMetadata(directory / "OwnershipMetadata.nsproj", outputPath, output, error) == ExitCode::Success,
        "Stage 50.9 emits ownership-aware library metadata");
    tests.expect(error.empty() && output.find("format: NSMETA\n") != std::string::npos,
        "the metadata command identifies the metadata format without a numeric label");

    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    tests.expect(codec.read(outputPath, metadata, error),
        "NSMETA ownership contracts round-trip through the reader");
    tests.expect(!metadata.moduleName.empty(),
        "metadata reader preserves the module identity");

    const LibraryMetadataMember* plain = findMember(metadata, "Plain");
    const LibraryMetadataMember* take = findMember(metadata, "Take");
    const LibraryMetadataMember* view = findMember(metadata, "View");
    const LibraryMetadataMember* raw = findMember(metadata, "Raw");
    tests.expect(plain != nullptr && take != nullptr && view != nullptr && raw != nullptr,
        "plain, owned, borrowed and raw-pointer members are all persisted");
    if (plain == nullptr || take == nullptr || view == nullptr || raw == nullptr) return;

    tests.expect(
        plain->declaredOwnership == LibraryMetadataOwnershipKind::None &&
            plain->parameters.size() == 1 &&
            plain->parameters[0].ownership == LibraryMetadataOwnershipKind::None,
        "unqualified T persists an explicit None ownership contract");
    tests.expect(
        take->declaredOwnership == LibraryMetadataOwnershipKind::Owned &&
            take->parameters.size() == 1 &&
            take->parameters[0].ownership == LibraryMetadataOwnershipKind::Owned,
        "owned T persists Owned independently for return and parameter slots");
    tests.expect(
        view->declaredOwnership == LibraryMetadataOwnershipKind::Borrowed &&
            view->parameters.size() == 1 &&
            view->parameters[0].ownership == LibraryMetadataOwnershipKind::Borrowed,
                 "borrowed T persists Borrowed independently for return and "
                 "parameter slots");

    tests.expect(
        plain->declaredTypeKey == take->declaredTypeKey &&
            plain->declaredTypeKey == view->declaredTypeKey,
        "T, owned T and borrowed T share the same physical canonical typeKey");
    tests.expect(
        plain->parameters[0].typeKey == take->parameters[0].typeKey &&
            plain->parameters[0].typeKey == view->parameters[0].typeKey,
        "ownership does not contaminate physical parameter type identity");
    tests.expect(
        raw->declaredOwnership == LibraryMetadataOwnershipKind::None &&
            raw->declaredTypeKey.ends_with('*') &&
            raw->declaredTypeKey != plain->declaredTypeKey,
        "T* remains physically distinct from T/owned T/borrowed T");
    tests.expect(
        take->signatureKey != plain->signatureKey && view->signatureKey != plain->signatureKey &&
            take->signatureKey != view->signatureKey,
        "NativeSharp semantic member signatures remain ownership-distinct");

    tests.expect(!metadata.dispatchTables.empty(),
        "ownership-aware class metadata publishes a destruction dispatch contract");
    if (!metadata.dispatchTables.empty())
    {
        metadata.dispatchTables.front().destructionEntryName.clear();
        error.clear();
        tests.expect(!codec.verifyCurrentCompiler(metadata, error) &&
                error.find("does not provide its producer-owned destruction entry") != std::string::npos,
            "current NSMETA fails closed when cross-module drop glue is missing");
    }
}

void testSemanticFingerprintSensitivity(TestContext& tests, const std::filesystem::path& root)
{
    const auto plainDirectory = root / "FingerprintPlain";
    const auto borrowedDirectory = root / "FingerprintBorrowed";
    constexpr std::string_view projectName = "FingerprintContract";

    writeProject(plainDirectory, projectName, R"NS(
public class Resource { public int Value; }
public class Api
{
    public static Resource Echo(Resource value) { return value; }
}
)NS");
    writeProject(borrowedDirectory, projectName, R"NS(
public class Resource { public int Value; }
public class Api
{
    public static borrowed Resource Echo(borrowed Resource value) { return value; }
}
)NS");

    std::string output;
    std::string error;
    const auto plainPath = plainDirectory / "plain.nsmeta";
    const auto borrowedPath = borrowedDirectory / "borrowed.nsmeta";
    tests.expect(emitMetadata(plainDirectory / "FingerprintContract.nsproj", plainPath, output, error) == ExitCode::Success,
        "unqualified semantic-fingerprint fixture emits metadata");
    output.clear(); error.clear();
    tests.expect(emitMetadata(borrowedDirectory / "FingerprintContract.nsproj", borrowedPath, output, error) == ExitCode::Success,
        "borrowed semantic-fingerprint fixture emits metadata");

    LibraryMetadataCodec codec;
    LibraryMetadata plain;
    LibraryMetadata borrowed;
    tests.expect(codec.read(plainPath, plain, error) && codec.read(borrowedPath, borrowed, error),
        "both semantic-fingerprint fixtures read successfully");
    tests.expect(plain.abiFingerprint != borrowed.abiFingerprint,
                 "ownership-only contract changes invalidate the NativeSharp "
                 "compile-time ABI/semantic fingerprint");
}

void testStableCAbiProjection(TestContext& tests, const std::filesystem::path& root)
{
    const auto plainDirectory = root / "HostPlain";
    const auto ownedDirectory = root / "HostOwned";
    constexpr std::string_view projectName = "StableHost";

    writeProject(plainDirectory, projectName, R"NS(
public class Resource
{
    public int Value;
    public Resource(int value) { Value = value; }
    ~Resource() { Value = 0; }
}
public class Api
{
    public static export extern C Resource Echo(Resource value) { return value; }
}
)NS");
    writeProject(ownedDirectory, projectName, R"NS(
public class Resource
{
    public int Value;
    public Resource(int value) { Value = value; }
    ~Resource() { Value = 0; }
}
public class Api
{
    public static export extern C owned Resource Echo(owned Resource value) { return move(value); }
}
)NS");

    std::string output;
    std::string error;
    const auto plainHeader = plainDirectory / "StableHost.h";
    const auto ownedHeader = ownedDirectory / "StableHost.h";
    tests.expect(emitHeader(plainDirectory / "StableHost.nsproj", plainHeader, output, error) == ExitCode::Success,
        "unqualified C-host header fixture emits successfully");
    output.clear(); error.clear();
    tests.expect(emitHeader(ownedDirectory / "StableHost.nsproj", ownedHeader, output, error) == ExitCode::Success,
        "owned C-host header fixture emits successfully");

    const std::string plainText = readFile(plainHeader);
    const std::string ownedText = readFile(ownedHeader);
    tests.expect(!plainText.empty() && plainText == ownedText,
        "ownership qualifiers do not change the generated Stage 48 C header");
    tests.expect(ownedText.find("owned") == std::string::npos && ownedText.find("borrowed") == std::string::npos,
        "NativeSharp ownership syntax never leaks into the C host header");

    const auto ownedMetadataPath = ownedDirectory / "StableHost.nsmeta";
    output.clear(); error.clear();
    tests.expect(emitMetadata(ownedDirectory / "StableHost.nsproj", ownedMetadataPath, output, error) == ExitCode::Success,
        "owned C ABI fixture also emits NativeSharp metadata");
    LibraryMetadataCodec codec;
    LibraryMetadata ownedMetadata;
    tests.expect(codec.read(ownedMetadataPath, ownedMetadata, error),
        "owned C ABI fixture metadata reads successfully");
    const LibraryMetadataMember* echo = findMember(ownedMetadata, "Echo");
    tests.expect(echo != nullptr && echo->declaredOwnership == LibraryMetadataOwnershipKind::Owned &&
            echo->parameters.size() == 1 && echo->parameters[0].ownership == LibraryMetadataOwnershipKind::Owned,
                 "NSMETA retains ownership even when the C projection intentionally "
                 "erases it");
    tests.expect(ownedMetadata.exports.size() == 1 && echo != nullptr &&
            ownedMetadata.exports[0].returnTypeKey == echo->declaredTypeKey &&
            ownedMetadata.exports[0].parameterTypeKeys.size() == 1 &&
            ownedMetadata.exports[0].parameterTypeKeys[0] == echo->parameters[0].typeKey,
                 "C export metadata remains a physical type projection with the "
                 "Stage 48 ABI shape");
}

// Verify deterministic writes and actual decoding after varying reserved header bytes.
bool reservedBytesPreserveContent(
    LibraryMetadataCodec& codec,
    const LibraryMetadata& model,
    const std::filesystem::path& path,
    std::string& error)
{
    return nativesharp::tests::metadataReservedBytesPreserveContent(codec, model, path, error);
}

void testBackwardCompatibleRead(TestContext& tests, const std::filesystem::path& root)
{
    nativesharp::interop::LibraryMetadataCodec codec;
    std::string error;

    nativesharp::interop::LibraryMetadata older;
    older.producerVersion = std::string(nativesharp::kCompilerVersion);
    older.moduleName = "OldFormat";
    older.targetTriple = "x86_64-pc-windows-msvc";

    nativesharp::interop::LibraryMetadataType enumType;
    enumType.kind = nativesharp::interop::LibraryMetadataTypeKind::Enum;
    enumType.identityKey = "OldFormat.Mode";
    enumType.fullyQualifiedName = "OldFormat.Mode";
    enumType.size = 4;
    enumType.alignment = 4;
    older.types.push_back(enumType);

    // Un segundo tipo DETRAS del enum: si el lector se comiera bytes de mas al
    // decodificar el primero, este saldria corrido o el fichero se rompería. Un
    // solo tipo no probaria nada.
    nativesharp::interop::LibraryMetadataType structType;
    structType.kind = nativesharp::interop::LibraryMetadataTypeKind::Struct;
    structType.identityKey = "OldFormat.Pair";
    structType.fullyQualifiedName = "OldFormat.Pair";
    structType.size = 8;
    structType.alignment = 4;
    older.types.push_back(structType);

    const auto path = root / "OldFormat.nsmeta";
    std::filesystem::create_directories(root);
    tests.expect(codec.writeModel(older, path, error), "a metadata file can be produced for the content test");

    nativesharp::interop::LibraryMetadata decoded;
    error.clear();
    tests.expect(codec.read(path, decoded, error),
        "the current reader decodes an NSMETA payload");
    tests.expect(reservedBytesPreserveContent(codec, older, path, error),
        "reserved bytes do not alter the decoded content (" + error + ")");
    tests.expect(decoded.types.size() == 2, "both type records survive the older payload");
    tests.expect(decoded.types.size() == 2 && decoded.types[0].identityKey == "OldFormat.Mode" &&
            decoded.types[0].size == 4 && decoded.types[0].enumUnderlyingTypeKey.empty(),
                 "an enum without an underlying type key remains unchanged, and the "
                 "reader does not invent one");
    tests.expect(decoded.types.size() == 2 && decoded.types[1].identityKey == "OldFormat.Pair" &&
            decoded.types[1].size == 8,
                 "the record after the enum remains aligned");
    tests.expect(codec.verifyCurrentCompiler(decoded, error),
        "decoded metadata is accepted by the compiler");

    // A parameter and a following member detect any record alignment errors.
    nativesharp::interop::LibraryMetadata previous;
    previous.producerVersion = std::string(nativesharp::kCompilerVersion);
    previous.moduleName = "PreviousFormat";
    previous.targetTriple = "x86_64-pc-windows-msvc";
    nativesharp::interop::LibraryMetadataMember callable;
    callable.name = "Optional";
    nativesharp::interop::LibraryMetadataParameter oldParameter;
    oldParameter.name = "value";
    oldParameter.typeKey = "builtin:int";
    oldParameter.defaultValueKind =
        nativesharp::interop::LibraryMetadataDefaultValueKind::Integer;
    oldParameter.defaultValueFlags = 1U;
    oldParameter.defaultValueBits = 7U;
    callable.parameters.push_back(oldParameter);
    previous.members.push_back(callable);
    nativesharp::interop::LibraryMetadataMember sentinel;
    sentinel.name = "AfterParameter";
    sentinel.constantKind = 1U;
    sentinel.constantBits = 99U;
    previous.members.push_back(sentinel);

    const auto previousPath = root / "PreviousFormat.nsmeta";
    error.clear();
    tests.expect(codec.writeModel(previous, previousPath, error),
        "an NSMETA file with a parameter record can still be produced");
    nativesharp::interop::LibraryMetadata previousDecoded;
    error.clear();
    tests.expect(codec.read(previousPath, previousDecoded, error),
        "the current reader decodes the immediate NSMETA predecessor");
    const LibraryMetadataMember* previousCallable = memberNamed(previousDecoded, "Optional");
    const LibraryMetadataMember* previousSentinel = memberNamed(previousDecoded, "AfterParameter");
    // The default must survive the round trip exactly.
    tests.expect(previousDecoded.members.size() == 2 && previousCallable != nullptr &&
            previousCallable->parameters.size() == 1 &&
            previousCallable->parameters[0].defaultValueKind ==
                nativesharp::interop::LibraryMetadataDefaultValueKind::Integer &&
            previousCallable->parameters[0].defaultValueFlags == 1U &&
            previousCallable->parameters[0].defaultValueBits == 7U,
        "a parameter default survives the round trip");
    tests.expect(reservedBytesPreserveContent(codec, previous, previousPath, error),
        "reserved bytes preserve parameter metadata (" + error + ")");
    tests.expect(previousSentinel != nullptr && previousSentinel->constantBits == 99U,
        "the member after a parameter record remains byte-aligned");

    // Pass kind and parameter defaults must survive together without shifting later records.
    nativesharp::interop::LibraryMetadata passPrevious = previous;
    passPrevious.members[0].parameters[0].passKind = LibraryMetadataParameterPassKind::Ref;
    const auto passPreviousPath = root / "PreviousPassKindFormat.nsmeta";
    error.clear();
    tests.expect(codec.writeModel(passPrevious, passPreviousPath, error),
                 "a parameter record with pass kind can be produced");
    nativesharp::interop::LibraryMetadata passPreviousDecoded;
    error.clear();
    const bool passPreviousRead = codec.read(passPreviousPath, passPreviousDecoded, error);
    const LibraryMetadataMember* passCallable = memberNamed(passPreviousDecoded, "Optional");
    const LibraryMetadataMember* passSentinel = memberNamed(passPreviousDecoded, "AfterParameter");
    tests.expect(passPreviousRead && passPreviousDecoded.members.size() == 2 &&
            passCallable != nullptr && passCallable->parameters.size() == 1,
        "the current reader decodes the immediate NSMETA predecessor");
    tests.expect(passCallable != nullptr && !passCallable->parameters.empty() &&
            passCallable->parameters[0].passKind ==
            LibraryMetadataParameterPassKind::Ref &&
            passCallable->parameters[0].defaultValueKind ==
                nativesharp::interop::LibraryMetadataDefaultValueKind::Integer &&
            passSentinel != nullptr,
                 "a ref parameter keeps its pass kind and default, and "
                 "following records stay aligned");
    tests.expect(reservedBytesPreserveContent(codec, passPrevious, passPreviousPath, error),
        "reserved bytes preserve pass-kind metadata (" + error + ")");

    // Delegate signatures must not shift the member records following them.
    nativesharp::interop::LibraryMetadata delegatePrevious = previous;
    nativesharp::interop::LibraryMetadataType oldDelegate;
    oldDelegate.kind = nativesharp::interop::LibraryMetadataTypeKind::Delegate;
    oldDelegate.identityKey = "Previous::Transform";
    oldDelegate.fullyQualifiedName = "Previous.Transform";
    oldDelegate.delegateFlags = 1U;
    oldDelegate.delegateReturnTypeKey = "builtin:int";
    oldDelegate.delegateParameterTypeKeys.push_back("builtin:int");
    delegatePrevious.types.push_back(oldDelegate);
    const auto delegatePreviousPath = root / "PreviousDelegateFormat.nsmeta";
    error.clear();
    tests.expect(codec.writeModel(delegatePrevious, delegatePreviousPath, error),
                 "a type record with a delegate signature can be produced");
    nativesharp::interop::LibraryMetadata delegatePreviousDecoded;
    error.clear();
    tests.expect(codec.read(delegatePreviousPath, delegatePreviousDecoded, error) &&
            delegatePreviousDecoded.types.size() == 1 &&
            delegatePreviousDecoded.types[0].delegateReturnTypeKey == "builtin:int" &&
            delegatePreviousDecoded.types[0].delegateParameterTypeKeys == oldDelegate.delegateParameterTypeKeys &&
            delegatePreviousDecoded.members.size() == 2,
                 "a delegate signature survives and members stay aligned");
    tests.expect(reservedBytesPreserveContent(codec, delegatePrevious, delegatePreviousPath, error),
        "reserved bytes preserve delegate metadata (" + error + ")");

    // Descriptor accessors, vtable names and slots must all survive the round trip.
    nativesharp::interop::LibraryMetadata descriptorPrevious = previous;
    nativesharp::interop::LibraryMetadataType oldWidget;
    oldWidget.kind = nativesharp::interop::LibraryMetadataTypeKind::Class;
    oldWidget.identityKey = "Previous::Widget";
    oldWidget.fullyQualifiedName = "Previous.Widget";
    oldWidget.referenceSize = descriptorPrevious.pointerSize;
    oldWidget.referenceAlignment = descriptorPrevious.pointerAlignment;
    descriptorPrevious.types.push_back(oldWidget);
    nativesharp::interop::LibraryMetadataDispatch oldDispatch;
    oldDispatch.typeKey = "type:" + oldWidget.identityKey;
    oldDispatch.stableTypeId = 42U;
    oldDispatch.descriptorName = "__nsc_classdesc_previous_widget";
    oldDispatch.descriptorAccessorName = "__nsc_get_classdesc_previous_widget";
    oldDispatch.vtableName = "__nsc_vtable_previous_widget";
    oldDispatch.destructionEntryName = "__nsc_destroy_previous_widget";
    oldDispatch.slotSignatures.push_back("Previous.Widget.Value():int");
    oldDispatch.slotCalleeNames.push_back("__nsc_previous_widget_value");
    descriptorPrevious.dispatchTables.push_back(oldDispatch);
    const auto descriptorPreviousPath = root / "PreviousDescriptorAccessorFormat.nsmeta";
    error.clear();
    tests.expect(codec.writeModel(descriptorPrevious, descriptorPreviousPath, error),
                 "a dispatch record with an accessor can be produced");
    nativesharp::interop::LibraryMetadata descriptorPreviousDecoded;
    error.clear();
    tests.expect(codec.read(descriptorPreviousPath, descriptorPreviousDecoded, error) &&
            descriptorPreviousDecoded.dispatchTables.size() == 1 &&
            descriptorPreviousDecoded.dispatchTables[0].descriptorAccessorName ==
                oldDispatch.descriptorAccessorName &&
            descriptorPreviousDecoded.dispatchTables[0].vtableName == oldDispatch.vtableName &&
            descriptorPreviousDecoded.dispatchTables[0].slotCalleeNames == oldDispatch.slotCalleeNames,
                 "the descriptor accessor survives without shifting the vtable "
                 "or slots");
    tests.expect(reservedBytesPreserveContent(codec, descriptorPrevious, descriptorPreviousPath, error),
        "reserved bytes preserve dispatch metadata (" + error + ")");

    // Keep a following specialization as an alignment sentinel for the template record.
    nativesharp::interop::LibraryMetadata templatePrevious = descriptorPrevious;
    nativesharp::interop::LibraryMetadataGenericTemplate oldTemplate;
    oldTemplate.kind = 1U;
    oldTemplate.accessibility = 3U;
    oldTemplate.canonicalKey = "Previous::Box`1";
    oldTemplate.moduleName = "Previous";
    oldTemplate.namespaceName = "Previous";
    oldTemplate.name = "Box";
    oldTemplate.parameters.push_back("T");
    oldTemplate.sourceText = "public struct Box<T> { public T Value; }";
    templatePrevious.genericTemplates.push_back(oldTemplate);
    nativesharp::interop::LibraryMetadataGenericSpecialization oldSpecialization;
    oldSpecialization.kind = 1U;
    oldSpecialization.templateKey = oldTemplate.canonicalKey;
    oldSpecialization.specializationKey = "Previous::Box`1<int>";
    oldSpecialization.specializedName = "Box__1_int";
    oldSpecialization.typeArguments.push_back("int");
    templatePrevious.genericSpecializations.push_back(oldSpecialization);
    const auto templatePreviousPath = root / "PreviousGenericTemplateFormat.nsmeta";
    error.clear();
    tests.expect(codec.writeModel(templatePrevious, templatePreviousPath, error),
                 "a generic template with materialization fields can be produced");
    nativesharp::interop::LibraryMetadata templatePreviousDecoded;
    error.clear();
    tests.expect(codec.read(templatePreviousPath, templatePreviousDecoded, error) &&
            templatePreviousDecoded.genericTemplates.size() == 1 &&
            templatePreviousDecoded.genericTemplates[0].accessibility == oldTemplate.accessibility &&
            templatePreviousDecoded.genericTemplates[0].sourceText == oldTemplate.sourceText &&
            templatePreviousDecoded.genericSpecializations.size() == 1 &&
            templatePreviousDecoded.genericSpecializations[0].specializedName ==
                oldSpecialization.specializedName,
                 "template accessibility and declaration survive without "
                 "shifting following specializations");
    tests.expect(reservedBytesPreserveContent(codec, templatePrevious, templatePreviousPath, error),
        "reserved bytes preserve template metadata (" + error + ")");

    // The binary contract and subsequent generic records must remain aligned.
    nativesharp::interop::LibraryMetadata contractPrevious = templatePrevious;
    const auto contractPreviousPath = root / "PreviousAbiContractFormat.nsmeta";
    error.clear();
    tests.expect(codec.writeModel(contractPrevious, contractPreviousPath, error),
                 "metadata with the same ABI contract can be produced");
    nativesharp::interop::LibraryMetadata contractPreviousDecoded;
    error.clear();
    tests.expect(codec.read(contractPreviousPath, contractPreviousDecoded, error) &&
            contractPreviousDecoded.nativeAbiMajor == contractPrevious.nativeAbiMajor &&
            contractPreviousDecoded.runtimeAbiMajor == contractPrevious.runtimeAbiMajor &&
            contractPreviousDecoded.delegateAbiVersion == contractPrevious.delegateAbiVersion &&
            contractPreviousDecoded.managedDelegateCellSize == contractPrevious.managedDelegateCellSize &&
            contractPreviousDecoded.genericTemplates.size() == 1 &&
            contractPreviousDecoded.genericTemplates[0].sourceText == oldTemplate.sourceText &&
            contractPreviousDecoded.genericSpecializations.size() == 1,
                 "the ABI contract survives without shifting "
                 "later records");
    tests.expect(reservedBytesPreserveContent(codec, contractPrevious, contractPreviousPath, error),
        "reserved bytes preserve ABI and template metadata (" + error + ")");
    error.clear();
    // An incomplete template is rejected; completing its parameter contract is the positive control.
    const bool contractPreviousAccepted = codec.verifyCurrentCompiler(contractPreviousDecoded, error);
    tests.expect(!contractPreviousAccepted &&
            error.find("parameter-contract count that differs from its generic arity") != std::string::npos,
        "a template without parameter contracts is refused for that, under any label (" + error + ")");
    nativesharp::interop::LibraryMetadataGenericTemplate::ParameterContract templateParameter;
    templateParameter.name = "T";
    templateParameter.kind = 0U;
    contractPreviousDecoded.genericTemplates[0].parameterContracts.push_back(templateParameter);
    error.clear();
    const bool completedContractAccepted = codec.verifyCurrentCompiler(contractPreviousDecoded, error);
    tests.expect(completedContractAccepted,
        "the same model is consumable once the template states its contract (" + error + ")");

    // LA HUELLA ABI TIENE QUE DISTINGUIR EL SIGNO. `enum : byte` y `enum : sbyte`
    // miden lo mismo, asi que si la huella solo cubriera el tamano dos modulos
    // incompatibles tendrian la MISMA, y el mecanismo que existe para detectar un
    // cambio de ABI dejaria pasar justo este.
    const auto fingerprintFor = [&](std::string_view underlying) {
        nativesharp::interop::LibraryMetadata one;
        one.producerVersion = std::string(nativesharp::kCompilerVersion);
        one.moduleName = "Sign";
        one.targetTriple = "x86_64-pc-windows-msvc";
        nativesharp::interop::LibraryMetadataType t;
        t.kind = nativesharp::interop::LibraryMetadataTypeKind::Enum;
        t.identityKey = "Sign.E";
        t.fullyQualifiedName = "Sign.E";
        t.size = 1;
        t.alignment = 1;
        t.enumUnderlyingTypeKey = std::string(underlying);
        one.types.push_back(t);
        const auto where = root / (std::string("sign-") + std::string(underlying.substr(8)) + ".nsmeta");
        std::string localError;
        nativesharp::interop::LibraryMetadata back;
        return codec.writeModel(one, where, localError) && codec.read(where, back, localError)
            ? nativesharp::interop::LibraryMetadataCodec::fingerprintHex(back.abiFingerprint)
            : std::string{};
    };
    const std::string unsignedFingerprint = fingerprintFor("builtin:byte");
    const std::string signedFingerprint = fingerprintFor("builtin:sbyte");
    tests.expect(!unsignedFingerprint.empty() && unsignedFingerprint != signedFingerprint,
                 "`enum : byte` and `enum : sbyte` get different ABI "
                 "fingerprints despite the same size");

    // EL OBJETIVO SE VALIDA, NO SE SUSTITUYE. `writeModel` existe para medir
    // round-trips, asi que un campo que se sobrescribiera en silencio con la
    // constante haria que esta misma prueba dijera "el objetivo sobrevivio"
    // cuando lo que sobrevivio es la constante -- una prueba que se miente a si
    // misma.
    tests.expect(decoded.targetId == 1 && decoded.pointerSize == 8 && decoded.pointerAlignment == 8,
        "the written model's target survives the round-trip");

    nativesharp::interop::LibraryMetadata foreign = older;
    foreign.targetId = 7;
    error.clear();
    tests.expect(
        !codec.writeModel(foreign, root / "foreign.nsmeta", error) &&
            error.find("Windows x64") != std::string::npos,
                 "a model naming another target is refused, not silently "
                 "rewritten to Windows x64");

    nativesharp::interop::LibraryMetadata narrowPointer = older;
    narrowPointer.pointerSize = 4;
    error.clear();
    tests.expect(
        !codec.writeModel(narrowPointer, root / "narrow.nsmeta", error) &&
            error.find("Windows x64") != std::string::npos,
        "a model with a foreign pointer width is refused too");
}

void testContentCompatibilityGate(TestContext& tests, const std::filesystem::path& root)
{
    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    metadata.targetId = 1;
    metadata.pointerSize = 8;
    metadata.pointerAlignment = 8;
    metadata.producerVersion = std::string(nativesharp::kCompilerVersion);
    metadata.targetTriple = "x86_64-pc-windows-msvc";
    std::string error;
    // A model without module identity is invalid; adding the identity is the positive control.
    const bool minimalAccepted = codec.verifyCurrentCompiler(metadata, error);
    tests.expect(!minimalAccepted && error.find("metadata module identity is empty") != std::string::npos,
                 "a model without module identity is rejected (" + error + ")");
    metadata.moduleName = "MinimalModule";
    for (const std::uint16_t reserved : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{65535}})
    {
        error.clear();
        const auto path = root / ("header-bytes-" + std::to_string(reserved) + ".nsmeta");
        tests.expect(codec.verifyCurrentCompiler(metadata, error) &&
                nativesharp::tests::metadataSurvivesReservedBytes(codec, metadata, reserved, path, error),
            "complete metadata survives varied reserved header bytes (" + error + ")");
    }

    // An incomplete generic declaration must fail independently of the valid module identity.
    metadata.moduleName = "CompatibilityGate";
    nativesharp::interop::LibraryMetadataGenericTemplate incompleteTemplate;
    incompleteTemplate.kind = 1U;
    incompleteTemplate.accessibility = 3U;
    incompleteTemplate.canonicalKey = "Current::Box`1";
    incompleteTemplate.moduleName = "Current";
    incompleteTemplate.name = "Box";
    incompleteTemplate.parameters.push_back("T");
    metadata.genericTemplates.push_back(incompleteTemplate);
    error.clear();
    tests.expect(!codec.verifyCurrentCompiler(metadata, error) &&
            error.find("materializable declaration") != std::string::npos,
                 "current NSMETA rejects a public generic template with no "
                 "materializable declaration");
    metadata.genericTemplates[0].sourceText = "public struct Box<T> { public T Value; }";
    nativesharp::interop::LibraryMetadataGenericTemplate::ParameterContract typeParameter;
    typeParameter.name = "T";
    typeParameter.kind = 0U;
    metadata.genericTemplates[0].parameterContracts.push_back(std::move(typeParameter));
    error.clear();
    tests.expect(codec.verifyCurrentCompiler(metadata, error),
                 "current NSMETA accepts the same public template once its "
                 "declaration slice is complete");
    metadata.genericTemplates.clear();

    // A complete model stays readable when its reserved header bytes vary.
    error.clear();
    tests.expect(codec.verifyCurrentCompiler(metadata, error) &&
            nativesharp::tests::metadataSurvivesReservedBytes(
                codec, metadata, 65535U, root / "content-header-control.nsmeta", error),
        "complete metadata remains readable with varied reserved header bytes (" + error + ")");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage50_9_metadata_host_compatibility_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testOwnershipRoundTrip(tests, root);
    testSemanticFingerprintSensitivity(tests, root);
    testStableCAbiProjection(tests, root);
    testBackwardCompatibleRead(tests, root);
    testContentCompatibilityGate(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
