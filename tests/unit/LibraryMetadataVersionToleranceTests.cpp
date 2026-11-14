#include "nativesharp/Basic/Version.h"
#include "nativesharp/Driver/CompatibilityPolicy.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace
{
using nativesharp::interop::LibraryMetadata;
using nativesharp::interop::LibraryMetadataCodec;
using nativesharp::tests::TestContext;

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

bool writeBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    return stream.good();
}

void expectReadable(
    TestContext& tests,
    const LibraryMetadataCodec& codec,
    const std::filesystem::path& path,
    std::string_view description)
{
    LibraryMetadata decoded;
    std::string error;
    const bool readable = codec.read(path, decoded, error);
    const bool compatible = readable && codec.verifyCurrentCompiler(decoded, error);
    tests.expect(compatible, std::string(description) + " (" + error + ")");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::current_path() / "_targeted" / "nsmeta-version-tolerance";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    LibraryMetadata model;
    model.producerVersion = std::string(nativesharp::kCompilerVersion);
    model.moduleName = "VersionTolerance";
    model.targetTriple = "x86_64-pc-windows-msvc";

    LibraryMetadataCodec codec;
    std::string error;
    const std::filesystem::path currentPath = root / "current.nsmeta";
    tests.expect(codec.writeModel(model, currentPath, error),
        "current metadata fixture writes (" + error + ")");
    expectReadable(tests, codec, currentPath, "current metadata is readable");

    LibraryMetadata withTraitImpl = model;
    nativesharp::interop::LibraryMetadataTraitImpl traitImpl;
    traitImpl.targetTypeKey = "type:External::Greeter";
    traitImpl.traitTypeKey = "type:VersionTolerance::Greeting";
    nativesharp::interop::LibraryMetadataMember traitMethod;
    traitMethod.kind = 2U;
    traitMethod.accessibility = 3U;
    traitMethod.declaringTypeKey = traitImpl.targetTypeKey;
    traitMethod.name = "Hello";
    traitMethod.signatureKey = "method:type:External::Greeter::Hello()";
    traitMethod.declaredTypeKey = "type:System::Void";
    traitMethod.mangledName = "VersionTolerance.Greeter.Hello";
    traitImpl.methods.push_back(std::move(traitMethod));
    withTraitImpl.traitImpls.push_back(std::move(traitImpl));
    const std::filesystem::path traitPath = root / "current-trait-impl.nsmeta";
    error.clear();
    tests.expect(codec.writeModel(withTraitImpl, traitPath, error),
        "current metadata with a 1.40 trait implementation writes (" + error + ")");
    LibraryMetadata currentDecoded;
    LibraryMetadata traitDecoded;
    error.clear();
    const bool currentRead = codec.read(currentPath, currentDecoded, error);
    const bool traitRead = codec.read(traitPath, traitDecoded, error);
    if (currentRead)
    {
        LibraryMetadata relabeled = currentDecoded;
        relabeled.formatMajor = 0;
        relabeled.formatMinor = 0;
        relabeled.payloadDecodedByCurrentReader = false;
        error.clear();
        tests.expect(codec.verifyCurrentCompiler(relabeled, error),
            "verification depends on content and ABI, not labels (" + error + ")");
        relabeled.pointerSize = 0;
        error.clear();
        tests.expect(!codec.verifyCurrentCompiler(relabeled, error) &&
                error.find("target ABI mismatch") != std::string::npos,
            "verification still rejects an actual incompatible target");
    }
    tests.expect(currentRead && traitRead && traitDecoded.traitImpls.size() == 1U &&
            traitDecoded.traitImpls.front().methods.size() == 1U,
        "the 1.40 trait-implementation section remains serialized and readable (" + error + ")");
    tests.expect(currentRead && traitRead &&
            currentDecoded.abiFingerprint != traitDecoded.abiFingerprint,
        "the 1.40 trait-implementation contract participates in the ABI fingerprint");

    LibraryMetadata relabeledTraitModel = withTraitImpl;
    relabeledTraitModel.formatMajor = 0;
    relabeledTraitModel.formatMinor = 0;
    const std::filesystem::path relabeledTraitPath = root / "trait-relabeled-model.nsmeta";
    error.clear();
    const bool wroteRelabeledTrait = codec.writeModel(relabeledTraitModel, relabeledTraitPath, error);
    LibraryMetadata readRelabeledTrait;
    tests.expect(wroteRelabeledTrait && codec.read(relabeledTraitPath, readRelabeledTrait, error) &&
            readRelabeledTrait.traitImpls.size() == 1U &&
            readRelabeledTrait.traitImpls.front().methods.size() == 1U,
        "writing preserves actual trait content regardless of model labels (" + error + ")");

    const std::filesystem::path archivedPath =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "fixtures" / "nsmeta" /
        "historical-1.37.nsmeta";
    tests.expect(std::filesystem::is_regular_file(archivedPath),
        "the immutable pre-1.40 metadata fixture exists");
    LibraryMetadata archivedModel;
    error.clear();
    const bool archivedRead = codec.read(archivedPath, archivedModel, error);
    const std::string archivedError = error;
    tests.expect(!archivedRead && !archivedError.empty(),
        "content that the general reader cannot interpret is rejected");
    LibraryMetadata incompatibleAbi = model;
    incompatibleAbi.objectAbiVersion = 0;
    error.clear();
    tests.expect(!codec.verifyCurrentCompiler(incompatibleAbi, error) &&
            error.find("object ABI") != std::string::npos,
        "an incompatible object ABI remains rejected based on its actual contract");

    const std::vector<std::uint8_t> current = readBytes(currentPath);
    tests.expect(current.size() >= 12U, "metadata fixture contains the fixed header");
    if (current.size() >= 12U)
    {
        LibraryMetadata arbitraryModel = model;
        arbitraryModel.formatMajor = 0;
        arbitraryModel.formatMinor = 0;
        const std::filesystem::path arbitraryModelPath = root / "arbitrary-model.nsmeta";
        error.clear();
        const bool wroteModel = codec.writeModel(arbitraryModel, arbitraryModelPath, error);
        tests.expect(wroteModel && readBytes(arbitraryModelPath) == current,
            "writing the same metadata content does not depend on model labels (" + error + ")");

        std::vector<std::uint8_t> arbitraryHeader = current;
        arbitraryHeader[8] = 0U;
        arbitraryHeader[9] = 0U;
        arbitraryHeader[10] = 0U;
        arbitraryHeader[11] = 0U;
        const std::filesystem::path arbitraryPath = root / "arbitrary-header.nsmeta";
        tests.expect(writeBytes(arbitraryPath, arbitraryHeader), "arbitrary-header fixture writes");
        expectReadable(tests, codec, arbitraryPath,
            "compatible metadata content is readable regardless of header labels");
        LibraryMetadata authenticated;
        error.clear();
        tests.expect(codec.readAuthenticated(arbitraryHeader, authenticated, error) &&
                codec.verifyCurrentCompiler(authenticated, error),
            "authenticated content is readable regardless of header labels (" + error + ")");
        if (current.size() > 12U)
        {
            std::vector<std::uint8_t> corrupted = current;
            corrupted.back() ^= 1U;
            LibraryMetadata rejected;
            error.clear();
            tests.expect(!codec.read(corrupted, rejected, error) &&
                    error.find("checksum") != std::string::npos,
                "changed payload bytes still fail integrity validation");
        }

        std::vector<std::uint8_t> differentMajor = current;
        differentMajor[8] = 2U;
        differentMajor[9] = 0U;
        const std::filesystem::path majorPath = root / "different-major.nsmeta";
        tests.expect(writeBytes(majorPath, differentMajor), "different-major fixture writes");
        expectReadable(tests, codec, majorPath,
            "a compatible payload is not rejected by its major label");

        std::vector<std::uint8_t> futureMinor = current;
        const std::uint16_t minor =
            static_cast<std::uint16_t>(nativesharp::interop::kMetadataFormatMinor + 1U);
        futureMinor[10] = static_cast<std::uint8_t>(minor & 0xffU);
        futureMinor[11] = static_cast<std::uint8_t>(minor >> 8U);
        const std::filesystem::path minorPath = root / "future-minor.nsmeta";
        tests.expect(writeBytes(minorPath, futureMinor), "future-minor fixture writes");
        expectReadable(tests, codec, minorPath,
            "a compatible payload is not rejected by its minor label");
    }

    std::vector<std::uint8_t> archived = readBytes(archivedPath);
    if (archived.size() >= 12U)
    {
        archived[8] = 0U;
        archived[9] = 0U;
        archived[10] = 0U;
        archived[11] = 0U;
        const std::filesystem::path archivedRelabeled = root / "archived-relabeled.nsmeta";
        tests.expect(writeBytes(archivedRelabeled, archived), "relabeled archived fixture writes");
        LibraryMetadata relabeledModel;
        error.clear();
        const bool relabeledRead = codec.read(archivedRelabeled, relabeledModel, error);
        tests.expect(relabeledRead == archivedRead && error == archivedError,
            "unreadable content fails identically regardless of header labels (" + error + ")");
    }

    model.producerVersion = "0.2.0-dev-old";
    error.clear();
    tests.expect(codec.verifyCurrentCompiler(model, error),
        "producer identity does not override compatible metadata contents (" + error + ")");
    model.producerVersion = "0.9.0";
    error.clear();
    tests.expect(codec.verifyCurrentCompiler(model, error),
        "a historical producer label does not override compatible metadata contents (" + error + ")");
    model.producerVersion = "1.0.1";
    error.clear();
    tests.expect(codec.verifyCurrentCompiler(model, error),
        "a previous producer label does not override compatible metadata contents (" + error + ")");

    std::string explanation;
    tests.expect(nativesharp::driver::checkCompilerCompatibility(
                     "99.37-experimental", explanation, error) &&
                     explanation.find("informational") != std::string::npos,
        "compiler release labels do not override capability validation");

    LibraryMetadata withPrivateDependency = currentDecoded;
    nativesharp::interop::LibraryMetadataBinaryDependency dependency;
    dependency.moduleName = "Dependency";
    dependency.libraryFileName = "Dependency.lib";
    dependency.metadataFileName = "Dependency.nsmeta";
    withPrivateDependency.binaryDependencies.push_back(std::move(dependency));
    const std::filesystem::path dependencyPath = root / "private-dependency.nsmeta";
    LibraryMetadata dependencyDecoded;
    error.clear();
    const bool dependencyRoundTrip = codec.writeModel(withPrivateDependency, dependencyPath, error) &&
        codec.read(dependencyPath, dependencyDecoded, error);
    tests.expect(dependencyRoundTrip && dependencyDecoded.binaryDependencies.size() == 1U,
        "private binary dependency is preserved in metadata (" + error + ")");
    tests.expect(dependencyRoundTrip && dependencyDecoded.abiFingerprint == currentDecoded.abiFingerprint,
        "private binary dependency does not change the public ABI fingerprint");

    LibraryMetadata withType = model;
    nativesharp::interop::LibraryMetadataType type;
    type.identityKey = "type:VersionTolerance::Item";
    type.fullyQualifiedName = "VersionTolerance.Item";
    nativesharp::interop::LibraryMetadataField field;
    field.name = "value";
    field.typeKey = "builtin:int";
    field.ownership = nativesharp::interop::LibraryMetadataOwnershipKind::Borrowed;
    field.bitOffset = 3U;
    field.bitWidth = 5U;
    type.fields.push_back(std::move(field));
    withType.types.push_back(std::move(type));
    nativesharp::interop::LibraryMetadataNamespaceFunction function;
    function.namespaceName = "VersionTolerance";
    function.name = "Use";
    function.signatureKey = "function:VersionTolerance::Use";
    function.declaredTypeKey = "builtin:void";
    function.mangledName = "VersionTolerance.Use";
    nativesharp::interop::LibraryMetadataParameter parameter;
    parameter.name = "items";
    parameter.typeKey = "builtin:int";
    parameter.isParameterPack = true;
    parameter.isDynamicTrait = true;
    function.parameters.push_back(std::move(parameter));
    withType.namespaceFunctions.push_back(std::move(function));
    const std::filesystem::path typePath = root / "type-fields.nsmeta";
    error.clear();
    const bool wroteType = codec.writeModel(withType, typePath, error);
    std::vector<std::uint8_t> typeBytes = readBytes(typePath);
    if (typeBytes.size() >= 12U)
    {
        typeBytes[8] = typeBytes[9] = typeBytes[10] = typeBytes[11] = 0U;
    }
    LibraryMetadata decodedType;
    const bool readType = wroteType && codec.read(typeBytes, decodedType, error);
    tests.expect(readType && decodedType.types.size() == 1U &&
            decodedType.types.front().fields.size() == 1U &&
            decodedType.types.front().fields.front().ownership ==
                nativesharp::interop::LibraryMetadataOwnershipKind::Borrowed &&
            decodedType.types.front().fields.front().bitOffset == 3U &&
            decodedType.types.front().fields.front().bitWidth == 5U,
        "type fields retain their content when header labels change (" + error + ")");
    tests.expect(readType && decodedType.namespaceFunctions.size() == 1U &&
            decodedType.namespaceFunctions.front().parameters.size() == 1U &&
            decodedType.namespaceFunctions.front().parameters.front().isParameterPack &&
            decodedType.namespaceFunctions.front().parameters.front().isDynamicTrait,
        "namespace function parameters retain their contracts when header labels change (" + error + ")");

    return tests.resultKeeping(root);
}
