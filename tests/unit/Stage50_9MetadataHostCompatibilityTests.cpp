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
#include <sstream>
#include <span>
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
    tests.expect(error.empty() && output.find("format: NSMETA 1.2") != std::string::npos,
        "Stage 50.9 advertises the NSMETA 1.2 schema");

    LibraryMetadataCodec codec;
    LibraryMetadata metadata;
    tests.expect(codec.read(outputPath, metadata, error),
        "NSMETA 1.2 ownership contracts round-trip through the reader");
    tests.expect(metadata.formatMajor == 1 && metadata.formatMinor == 2,
        "metadata reader preserves the 1.2 format identity");

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
        "borrowed T persists Borrowed independently for return and parameter slots");

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
        "ownership-only contract changes invalidate the NativeSharp compile-time ABI/semantic fingerprint");
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
        "NSMETA retains ownership even when the C projection intentionally erases it");
    tests.expect(ownedMetadata.exports.size() == 1 && echo != nullptr &&
            ownedMetadata.exports[0].returnTypeKey == echo->declaredTypeKey &&
            ownedMetadata.exports[0].parameterTypeKeys.size() == 1 &&
            ownedMetadata.exports[0].parameterTypeKeys[0] == echo->parameters[0].typeKey,
        "C export metadata remains a physical type projection with the Stage 48 ABI shape");
}

// UNA LECTURA RETROCOMPATIBLE DE VERDAD, no solo el guardia de versiones.
//
// `verifyCurrentCompiler` solo mira el numero: dice que un 1.1 es aceptable, pero no DECODIFICA
// nada. El riesgo real del minor 2 esta en el cuerpo -- un lector que leyera el campo nuevo sin
// mirar el minor se comeria los bytes del campo siguiente y a partir de ahi todo el registro
// saldria corrido. Eso solo se ve escribiendo un fichero de 1.1 y leyendolo con el lector de 1.2.
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

    // Un segundo tipo DETRAS del enum: si el lector se comiera bytes de mas al decodificar el
    // primero, este saldria corrido o el fichero se rompería. Un solo tipo no probaria nada.
    nativesharp::interop::LibraryMetadataType structType;
    structType.kind = nativesharp::interop::LibraryMetadataTypeKind::Struct;
    structType.identityKey = "OldFormat.Pair";
    structType.fullyQualifiedName = "OldFormat.Pair";
    structType.size = 8;
    structType.alignment = 4;
    older.types.push_back(structType);

    const auto path = root / "OldFormat.nsmeta";
    std::filesystem::create_directories(root);
    tests.expect(codec.writeModel(older, path, error),
        "a 1.1 metadata file can be produced, so backward compatibility can be measured");

    nativesharp::interop::LibraryMetadata decoded;
    error.clear();
    tests.expect(codec.read(path, decoded, error),
        "the current reader decodes an NSMETA 1.1 payload");
    tests.expect(decoded.formatMinor == 1, "the decoded file keeps its own 1.1 format identity");
    tests.expect(decoded.types.size() == 2, "both type records survive the older payload");
    tests.expect(decoded.types.size() == 2 && decoded.types[0].identityKey == "OldFormat.Mode" &&
            decoded.types[0].size == 4 && decoded.types[0].enumUnderlyingTypeKey.empty(),
        "an enum from 1.1 carries no underlying type key, and the reader does not invent one");
    tests.expect(decoded.types.size() == 2 && decoded.types[1].identityKey == "OldFormat.Pair" &&
            decoded.types[1].size == 8,
        "the record AFTER the enum is not shifted -- the 1.2 field was correctly not read");
    tests.expect(codec.verifyCurrentCompiler(decoded, error),
        "a decoded 1.1 file is accepted by the current compiler");

    // LA HUELLA ABI TIENE QUE DISTINGUIR EL SIGNO. `enum : byte` y `enum : sbyte` miden lo mismo,
    // asi que si la huella solo cubriera el tamano dos modulos incompatibles tendrian la MISMA, y
    // el mecanismo que existe para detectar un cambio de ABI dejaria pasar justo este.
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
        "`enum : byte` and `enum : sbyte` get different ABI fingerprints despite the same size");

    // EL OBJETIVO SE VALIDA, NO SE SUSTITUYE. `writeModel` existe para medir round-trips, asi que
    // un campo que se sobrescribiera en silencio con la constante haria que esta misma prueba
    // dijera "el objetivo sobrevivio" cuando lo que sobrevivio es la constante -- una prueba que
    // se miente a si misma.
    tests.expect(decoded.targetId == 1 && decoded.pointerSize == 8 && decoded.pointerAlignment == 8,
        "the written model's target survives the round-trip");

    nativesharp::interop::LibraryMetadata foreign = older;
    foreign.targetId = 7;
    error.clear();
    tests.expect(
        !codec.writeModel(foreign, root / "foreign.nsmeta", error) &&
            error.find("Windows x64") != std::string::npos,
        "a model naming another target is refused, not silently rewritten to Windows x64");

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
        "current compiler accepts NSMETA 1.0 as the backward-compatible minor predecessor");

    metadata.formatMinor = 1;
    error.clear();
    tests.expect(codec.verifyCurrentCompiler(metadata, error),
        "current compiler accepts NSMETA 1.1 as a backward-compatible minor predecessor");

    // EL NUMERO DE ESTA LINEA SUBE CON CADA MINOR, y por eso lleva `kFormatMinor + 1` en vez de un
    // literal: escrito a mano, el dia que el formato pasa a 1.2 esta prueba deja de comprobar "un
    // minor del futuro" y pasa a comprobar el minor ACTUAL -- que es exactamente lo que ocurrio al
    // subir de 1.1 a 1.2, y por lo que esta comprobacion salio roja.
    metadata.formatMinor = static_cast<std::uint16_t>(nativesharp::interop::kMetadataFormatMinor + 1);
    error.clear();
    tests.expect(!codec.verifyCurrentCompiler(metadata, error) && error.find("format") != std::string::npos,
        "future NSMETA minor versions remain rejected until their fields are understood");
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
