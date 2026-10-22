#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Interop/LibraryMetadataMerger.h"

#include "nativesharp/Basic/ArtifactTransaction.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Compilation/Project.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include "Internal/TemplateSourceReplacement.h"
#include "Internal/TemplateSourcePiece.h"
#include "Internal/ByteReader.h"
#include "Internal/ByteWriter.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Internal/LibraryMetadataCodecSupport.h"

namespace nativesharp::interop
{
using namespace metadata_detail;

bool verifyLibraryMetadataInvariants(const LibraryMetadata& metadata, std::string& error) noexcept
{
    if ((metadata.languageFeatureFlags & ~kMetadataLanguageFeatureMask) != 0U)
    {
        error = "NativeSharp metadata invariant: reserved language feature bits are set";
        return false;
    }
    return verifyFrozenAbiContract(metadata, error);
}
bool LibraryMetadataCodec::write(
    const frontend::ProjectCompilation& compilation,
    const std::filesystem::path& destination,
    std::string& error) const
{
    LibraryMetadata ignored;
    return write(compilation, destination, ignored, error);
}

bool LibraryMetadataCodec::write(
    const frontend::ProjectCompilation& compilation,
    const std::filesystem::path& destination,
    LibraryMetadata& writtenModel,
    std::string& error) const
{
    error.clear();
    LibraryMetadata metadata;
    if (!buildMetadataModel(compilation, metadata, error)) return false;
    return writeModelImpl(metadata, destination, &writtenModel, error);
}

// Escribe un MODELO ya construido, en la version menor que el propio modelo
// dice.
//
// POR QUE EXISTE, y no es andamiaje de pruebas. La compatibilidad hacia atras
// de este formato solo se puede MEDIR si se puede producir un fichero de una
// version anterior: sin esto, "un lector de 1.2 lee un fichero de 1.1" es una
// afirmacion sobre codigo que nadie ejecuto nunca -- y este proyecto no cuenta
// esas. Cada minor futuro hereda la prueba sin escribir nada nuevo.
bool LibraryMetadataCodec::writeModel(
    const LibraryMetadata& model,
    const std::filesystem::path& destination,
    std::string& error) const
{
    return writeModelImpl(model, destination, nullptr, error);
}

bool LibraryMetadataCodec::writeModelImpl(
    const LibraryMetadata& model,
    const std::filesystem::path& destination,
    LibraryMetadata* writtenModel,
    std::string& error) const
{
    error.clear();
    LibraryMetadata metadata;
    if (!prepareWritableMetadata(model, metadata, error)) return false;
    if (!applyWritableMetadataDowngrade(metadata, error)) return false;
    return writeMetadataFile(metadata, destination, writtenModel, error);
}

bool LibraryMetadataCodec::read(
    const std::filesystem::path& source,
    LibraryMetadata& metadata,
    std::string& error) const
{
    error.clear();
    std::vector<std::uint8_t> bytes;
    if (!readMetadataBytes(source, bytes, error)) return false;
    return read(bytes, metadata, error);
}

bool LibraryMetadataCodec::read(
    std::span<const std::uint8_t> bytes,
    LibraryMetadata& metadata,
    std::string& error) const
{
    error.clear();
    if (bytes.size() < kHeaderSize)
    {
        error = "NativeSharp metadata is truncated before its fixed header";
        return false;
    }

    ByteReader reader(bytes);
    std::uint16_t major = 0;
    std::uint16_t minor = 0;
    std::uint32_t targetId = 0;
    std::uint32_t pointerSize = 0;
    std::uint32_t pointerAlignment = 0;
    std::uint64_t payloadSize = 0;
    std::array<std::uint8_t, 32> abiFingerprint{};
    std::array<std::uint8_t, 32> payloadHash{};
    if (!readMetadataHeader(
            reader,
            major,
            minor,
            targetId,
            pointerSize,
            pointerAlignment,
            payloadSize,
            abiFingerprint,
            payloadHash,
            error))
        return false;
    if (payloadSize > kMaximumPayloadSize || payloadSize != reader.remaining())
    {
        error = "NativeSharp metadata payload length mismatch";
        return false;
    }
    const std::span<const std::uint8_t> payload(bytes.data() + kHeaderSize, static_cast<std::size_t>(payloadSize));
    if (!sameFingerprint(basic::sha256(payload), payloadHash))
    {
        error = "NativeSharp metadata payload checksum mismatch";
        return false;
    }

    LibraryMetadata decoded;
    initializeDecodedMetadata(
        decoded, major, minor, targetId, pointerSize, pointerAlignment, abiFingerprint);
    if (!authenticateDecodedMetadata(payload, decoded, error)) return false;
    normalizeDecodedMetadata(decoded);
    metadata = std::move(decoded);
    return true;
}

bool LibraryMetadataCodec::verifyCurrentCompiler(
    const LibraryMetadata& metadata,
    std::string& error) const
{
    error.clear();
    if (!verifyMetadataEnvelope(metadata, error)) return false;
    if (!verifyMetadataTypeDeclarations(metadata, error)) return false;
    if (!verifyMetadataReferenceAndDelegateContracts(metadata, error)) return false;
    if (!verifyMetadataDispatchContracts(metadata, error)) return false;
    std::unordered_set<std::string> declaredTypeKeys;
    std::unordered_set<std::string> specializedTypeKeys;
    std::unordered_set<std::string> externalTypeKeys;
    if (!verifyGenericAndExternalTypeCatalogues(
            metadata,
            declaredTypeKeys,
            specializedTypeKeys,
            externalTypeKeys,
            error))
    {
        return false;
    }
    if (!verifyAllMetadataTypeReferences(
            metadata,
            declaredTypeKeys,
            specializedTypeKeys,
            externalTypeKeys,
            error))
    {
        return false;
    }
    if (!verifyNamespaceAndExportCatalogues(metadata, error)) return false;
    if (!verifyBinaryClosureMetadata(metadata, error)) return false;
    if (!verifyVersionedMemberContracts(metadata, error)) return false;
    return verifyLifetimeMetadataContracts(metadata, error);
}

std::string LibraryMetadataCodec::fingerprintHex(const std::array<std::uint8_t, 32>& fingerprint)
{
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (const std::uint8_t byte : fingerprint)
    {
        stream << std::setw(2) << static_cast<unsigned>(byte);
    }
    return stream.str();
}

}
