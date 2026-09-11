#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

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
    const std::string metadataFormat =
        std::to_string(nativesharp::interop::kMetadataFormatMajor) + "." +
        std::to_string(nativesharp::interop::kMetadataFormatMinor);
    tests.expect(error.empty() && output.find("format: NSMETA " + metadataFormat) != std::string::npos,
        "the metadata command advertises the current NSMETA schema");

    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    tests.expect(codec.read(outputPath, metadata, error),
        "NSMETA ownership contracts round-trip through the reader");
    tests.expect(metadata.formatMajor == nativesharp::interop::kMetadataFormatMajor &&
                     metadata.formatMinor == nativesharp::interop::kMetadataFormatMinor,
        "metadata reader preserves the current format identity");

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

// UNA LECTURA RETROCOMPATIBLE DE VERDAD, no solo el guardia de versiones.
//
// `verifyCurrentCompiler` solo mira el numero: dice que un 1.1 es aceptable,
// pero no DECODIFICA nada. El riesgo real del minor 2 esta en el cuerpo -- un
// lector que leyera el campo nuevo sin mirar el minor se comeria los bytes del
// campo siguiente y a partir de ahi todo el registro saldria corrido. Eso solo
// se ve escribiendo un fichero de 1.1 y leyendolo con el lector de 1.2.
void testBackwardCompatibleRead(TestContext& tests, const std::filesystem::path& root)
{
    nativesharp::interop::LibraryMetadataCodec codec;
    std::string error;

    nativesharp::interop::LibraryMetadata older;
    older.formatMinor = 1;                       // sin `enumUnderlyingTypeKey` en el cuerpo
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
    tests.expect(codec.writeModel(older, path, error), "a 1.1 metadata file can be produced, so backward compatibility "
                                                       "can be measured");

    nativesharp::interop::LibraryMetadata decoded;
    error.clear();
    tests.expect(codec.read(path, decoded, error),
        "the current reader decodes an NSMETA 1.1 payload");
    tests.expect(decoded.formatMinor == 1, "the decoded file keeps its own 1.1 format identity");
    tests.expect(decoded.types.size() == 2, "both type records survive the older payload");
    tests.expect(decoded.types.size() == 2 && decoded.types[0].identityKey == "OldFormat.Mode" &&
            decoded.types[0].size == 4 && decoded.types[0].enumUnderlyingTypeKey.empty(),
                 "an enum from 1.1 carries no underlying type key, and the "
                 "reader does not invent one");
    tests.expect(decoded.types.size() == 2 && decoded.types[1].identityKey == "OldFormat.Pair" &&
            decoded.types[1].size == 8,
                 "the record AFTER the enum is not shifted -- the 1.2 field was "
                 "correctly not read");
    tests.expect(codec.verifyCurrentCompiler(decoded, error),
        "a decoded 1.1 file is accepted by the current compiler");

    // NSMETA 1.3 added the default-value tuple to every parameter record.
    // Exercise the immediate predecessor with a parameter AND a following member:
    // if the reader consumes 1.3 bytes from a 1.2 record, the sentinel member
    // shifts and the payload cannot decode correctly.
    nativesharp::interop::LibraryMetadata previous;
    previous.formatMinor = 2;
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
        "an NSMETA 1.2 file with a parameter record can still be produced");
    nativesharp::interop::LibraryMetadata previousDecoded;
    error.clear();
    tests.expect(codec.read(previousPath, previousDecoded, error),
        "the current reader decodes the immediate NSMETA 1.2 predecessor");
    tests.expect(previousDecoded.members.size() == 2 &&
            previousDecoded.members[0].parameters.size() == 1 &&
            previousDecoded.members[0].parameters[0].defaultValueKind ==
                nativesharp::interop::LibraryMetadataDefaultValueKind::None,
        "a 1.2 parameter has no invented default value");
    tests.expect(previousDecoded.members.size() == 2 &&
            previousDecoded.members[1].name == "AfterParameter" &&
            previousDecoded.members[1].constantBits == 99U,
        "the member after a 1.2 parameter record remains byte-aligned");

    // NSMETA 1.4 adds pass-kind after ownership and before the existing default
    // tuple. A 1.3 record must therefore decode its default in the old position
    // and default the absent source pass kind to Value instead of consuming the
    // first default byte as ref/out/in.
    nativesharp::interop::LibraryMetadata passPrevious = previous;
    passPrevious.formatMinor = 3;
    passPrevious.members[0].parameters[0].passKind = LibraryMetadataParameterPassKind::Ref;
    const auto passPreviousPath = root / "PreviousPassKindFormat.nsmeta";
    error.clear();
    tests.expect(codec.writeModel(passPrevious, passPreviousPath, error),
                 "an NSMETA 1.3 parameter record can still be produced without a "
                 "pass-kind byte");
    nativesharp::interop::LibraryMetadata passPreviousDecoded;
    error.clear();
    tests.expect(codec.read(passPreviousPath, passPreviousDecoded, error) &&
            passPreviousDecoded.members.size() == 2 &&
            passPreviousDecoded.members[0].parameters.size() == 1,
        "the current reader decodes the immediate NSMETA 1.3 predecessor");
    tests.expect(passPreviousDecoded.members[0].parameters[0].passKind ==
            LibraryMetadataParameterPassKind::Value &&
            passPreviousDecoded.members[0].parameters[0].defaultValueKind ==
                nativesharp::interop::LibraryMetadataDefaultValueKind::Integer &&
            passPreviousDecoded.members[1].name == "AfterParameter",
                 "a 1.3 parameter invents no ref/out/in identity and leaves following "
                 "records aligned");

    // NSMETA 1.5 adds a delegate signature to the TYPE record. A 1.4 writer must
    // omit it and the current reader must not consume the following field count
    // as signature data.
    nativesharp::interop::LibraryMetadata delegatePrevious = previous;
    delegatePrevious.formatMinor = 4;
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
                 "an NSMETA 1.4 type record can still be produced without "
                 "delegate signature fields");
    nativesharp::interop::LibraryMetadata delegatePreviousDecoded;
    error.clear();
    tests.expect(codec.read(delegatePreviousPath, delegatePreviousDecoded, error) &&
            delegatePreviousDecoded.types.size() == 1 &&
            delegatePreviousDecoded.types[0].delegateReturnTypeKey.empty() &&
            delegatePreviousDecoded.members.size() == 2,
                 "the current reader decodes NSMETA 1.4 without inventing a delegate "
                 "signature or losing alignment");

    // NSMETA 1.6 added the descriptor accessor after descriptorName. A 1.5
    // producer writes the following vtable name in the old position; the current
    // reader must leave the accessor empty and still recover that vtable byte for
    // byte.
    nativesharp::interop::LibraryMetadata descriptorPrevious = previous;
    descriptorPrevious.formatMinor = 5;
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
                 "an NSMETA 1.5 dispatch record can still be produced without an accessor "
                 "field");
    nativesharp::interop::LibraryMetadata descriptorPreviousDecoded;
    error.clear();
    tests.expect(codec.read(descriptorPreviousPath, descriptorPreviousDecoded, error) &&
            descriptorPreviousDecoded.dispatchTables.size() == 1 &&
            descriptorPreviousDecoded.dispatchTables[0].descriptorAccessorName.empty() &&
            descriptorPreviousDecoded.dispatchTables[0].vtableName == oldDispatch.vtableName &&
            descriptorPreviousDecoded.dispatchTables[0].slotCalleeNames == oldDispatch.slotCalleeNames,
                 "the current reader decodes NSMETA 1.5 dispatch data without shifting "
                 "its vtable or slots");

    // NSMETA 1.7 added accessibility and the public declaration slice to every
    // generic-template record. Keep a following specialization as the alignment
    // sentinel for an actual 1.6 file.
    nativesharp::interop::LibraryMetadata templatePrevious = descriptorPrevious;
    templatePrevious.formatMinor = 6;
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
                 "an NSMETA 1.6 generic template can still be produced without "
                 "1.7 materialization fields");
    nativesharp::interop::LibraryMetadata templatePreviousDecoded;
    error.clear();
    tests.expect(codec.read(templatePreviousPath, templatePreviousDecoded, error) &&
            templatePreviousDecoded.genericTemplates.size() == 1 &&
            templatePreviousDecoded.genericTemplates[0].accessibility == 0U &&
            templatePreviousDecoded.genericTemplates[0].sourceText.empty() &&
            templatePreviousDecoded.genericSpecializations.size() == 1 &&
            templatePreviousDecoded.genericSpecializations[0].specializedName ==
                oldSpecialization.specializedName,
                 "the current reader decodes NSMETA 1.6 templates without shifting "
                 "following specializations");

    // NSMETA 1.8 is the first file that states the binary contract independently
    // from the file schema. A real 1.7 predecessor must remain readable, must not
    // invent those bytes and must leave the already-present generic records
    // aligned.
    nativesharp::interop::LibraryMetadata contractPrevious = templatePrevious;
    contractPrevious.formatMinor = 7;
    const auto contractPreviousPath = root / "PreviousAbiContractFormat.nsmeta";
    error.clear();
    tests.expect(codec.writeModel(contractPrevious, contractPreviousPath, error),
                 "an NSMETA 1.7 file can still be produced without the 1.8 "
                 "ABI-contract header");
    nativesharp::interop::LibraryMetadata contractPreviousDecoded;
    error.clear();
    tests.expect(codec.read(contractPreviousPath, contractPreviousDecoded, error) &&
            contractPreviousDecoded.nativeAbiMajor == 0 &&
            contractPreviousDecoded.runtimeAbiMajor == 0 &&
            contractPreviousDecoded.delegateAbiVersion == 0 &&
            contractPreviousDecoded.managedDelegateCellSize == 0 &&
            contractPreviousDecoded.genericTemplates.size() == 1 &&
            contractPreviousDecoded.genericTemplates[0].sourceText == oldTemplate.sourceText &&
            contractPreviousDecoded.genericSpecializations.size() == 1,
                 "the current reader decodes 1.7 without inventing an ABI contract or "
                 "shifting later records");
    error.clear();
    tests.expect(codec.verifyCurrentCompiler(contractPreviousDecoded, error),
        "legacy NSMETA 1.7 remains consumable under the compatibility policy");

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

void testMinorCompatibilityGate(TestContext& tests)
{
    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    metadata.formatMajor = 1;
    metadata.formatMinor = 0;
    metadata.targetId = 1;
    metadata.pointerSize = 8;
    metadata.pointerAlignment = 8;
    metadata.producerVersion = std::string(nativesharp::kCompilerVersion);
    metadata.targetTriple = "x86_64-pc-windows-msvc";
    std::string error;
    tests.expect(codec.verifyCurrentCompiler(metadata, error),
                 "current compiler accepts NSMETA 1.0 as the backward-compatible "
                 "minor predecessor");

    metadata.formatMinor = 1;
    error.clear();
    tests.expect(codec.verifyCurrentCompiler(metadata, error),
                 "current compiler accepts NSMETA 1.1 as a backward-compatible "
                 "minor predecessor");

    metadata.formatMinor = nativesharp::interop::kMetadataFormatMinor;
    // NSMETA 1.8 valida tambien la identidad estructural del modulo. Los
    // canarios 1.0/1.1 de arriba siguen siendo modelos minimos; el modelo actual
    // ya debe parecerse a un productor real para que el fallo siguiente mida
    // exclusivamente la declaracion generica.
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
    error.clear();
    tests.expect(codec.verifyCurrentCompiler(metadata, error),
                 "current NSMETA accepts the same public template once its "
                 "declaration slice is complete");
    metadata.genericTemplates.clear();

    // EL NUMERO DE ESTA LINEA SUBE CON CADA MINOR, y por eso lleva `kFormatMinor
    // + 1` en vez de un literal: escrito a mano, el dia que el formato pasa a 1.2
    // esta prueba deja de comprobar "un minor del futuro" y pasa a comprobar el
    // minor ACTUAL -- que es exactamente lo que ocurrio al subir de 1.1 a 1.2, y
    // por lo que esta comprobacion salio roja.
    metadata.formatMinor = static_cast<std::uint16_t>(nativesharp::interop::kMetadataFormatMinor + 1);
    error.clear();
    tests.expect(!codec.verifyCurrentCompiler(metadata, error) && error.find("format") != std::string::npos,
                 "future NSMETA minor versions remain rejected until their "
                 "fields are understood");
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
    testMinorCompatibilityGate(tests);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
