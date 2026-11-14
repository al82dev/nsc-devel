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

namespace nativesharp::interop::metadata_detail
{

bool sameFingerprint(
    const std::array<std::uint8_t, 32>& left,
    const std::array<std::uint8_t, 32>& right) noexcept
{
    std::uint8_t difference = 0;
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        difference = static_cast<std::uint8_t>(difference | (left[index] ^ right[index]));
    }
    return difference == 0;
}
bool writeBytesAtomically(
    const std::filesystem::path& destination,
    std::span<const std::uint8_t> bytes,
    std::string& error)
{
    return basic::writeFileTransactionally(destination, bytes, error);
}

std::string versionPair(std::uint16_t major, std::uint16_t minor)
{
    return std::to_string(major) + "." + std::to_string(minor);
}

// E5: una incompatibilidad ABI tiene que nombrar LA DECISION FISICA que
// difiere. El mensaje anterior agrupaba ocho campos bajo "ABI contract is not
// compatible"; era correcto para detener la compilacion, pero obligaba a abrir
// el fichero o depurar el lector para saber si habia cambiado el layout de
// objetos, la celda delegate o el transporte de excepciones. Esta funcion es la
// unica tabla de compatibilidad del contrato 1.0 y la comparten el escritor de
// modelos y el consumidor.
// EN: Object ABI mismatch text. A library from another object ABI must be rebuilt, never mixed.
// ES: Texto del desajuste de object ABI. Una biblioteca de otro object ABI se recompila, nunca se mezcla.
std::string objectAbiMismatchMessage(std::uint16_t found)
{
    return "NativeSharp ABI contract mismatch: object ABI expected " + std::to_string(kObjectAbiVersion) +
        ", found " + std::to_string(found) + "; recompile the library with the current compiler";
}

bool verifyFrozenAbiContract(const LibraryMetadata& metadata, std::string& error)
{
    if (metadata.nativeAbiMajor != kNativeAbiMajor || metadata.nativeAbiMinor > kNativeAbiMinor)
    {
        error = "NativeSharp ABI contract mismatch: native ABI expected " +
                versionPair(kNativeAbiMajor, kNativeAbiMinor) + ", found " +
                versionPair(metadata.nativeAbiMajor, metadata.nativeAbiMinor);
        return false;
    }
    if (metadata.runtimeAbiMajor != kRuntimeAbiMajor || metadata.runtimeAbiMinor > kRuntimeAbiMinor)
    {
        error = "NativeSharp ABI contract mismatch: runtime ABI expected " +
                versionPair(kRuntimeAbiMajor, kRuntimeAbiMinor) + ", found " +
                versionPair(metadata.runtimeAbiMajor, metadata.runtimeAbiMinor);
        return false;
    }
    const auto exactVersion = [&](std::string_view component, std::uint16_t found, std::uint16_t expected) {
        if (found == expected) return true;
        error = "NativeSharp ABI contract mismatch: " + std::string(component) + " expected " +
                std::to_string(expected) + ", found " + std::to_string(found);
        return false;
    };
    if (!exactVersion("mangling ABI", metadata.manglingAbiVersion, kManglingAbiVersion)) return false;
    if (metadata.objectAbiVersion != kObjectAbiVersion)
    {
        error = objectAbiMismatchMessage(metadata.objectAbiVersion);
        return false;
    }
    if (!exactVersion("delegate ABI", metadata.delegateAbiVersion, kDelegateAbiVersion) ||
        !exactVersion("exception ABI", metadata.exceptionAbiVersion, kExceptionAbiVersion))
    {
        return false;
    }
    if (metadata.managedDelegateCellSize != kManagedDelegateCellSize)
    {
        error = "NativeSharp ABI contract mismatch: managed delegate cell size "
                "expected " +
                std::to_string(kManagedDelegateCellSize) + " bytes, found " +
                std::to_string(metadata.managedDelegateCellSize) + " bytes";
        return false;
    }
    return true;
}

void normalizeWritableDelegateContracts(LibraryMetadata& metadata)
{
    for (LibraryMetadataType& type : metadata.types)
    {
        if (type.kind == LibraryMetadataTypeKind::Delegate &&
            type.delegateParameterDynamicTraits.empty())
        {
            type.delegateParameterDynamicTraits.resize(
                type.delegateParameterTypeKeys.size(), 0U);
        }
    }
    for (LibraryMetadataType& type : metadata.types)
    {
        if (type.kind != LibraryMetadataTypeKind::Delegate) continue;
        if (type.delegateParameterPassKinds.empty())
        {
            type.delegateParameterPassKinds.resize(
                type.delegateParameterTypeKeys.size(),
                LibraryMetadataParameterPassKind::Value);
        }
        if (type.delegateParameterLifetimeNames.empty())
            type.delegateParameterLifetimeNames.resize(
                type.delegateParameterTypeKeys.size());
    }
}

template<typename Callable>
bool normalizeWritableEscapeSummary(
    Callable& callable,
    std::string_view kind,
    std::string& error)
{
    if (callable.parameterEscapeSummary.empty())
    {
        callable.parameterEscapeSummary.resize(
            callable.parameters.size(), LibraryMetadataEscapeClass::Unknown);
    }
    if (callable.parameterEscapeSummary.size() == callable.parameters.size()) return true;
    error = "metadata " + std::string(kind) +
        " escape-summary parameter count does not match its signature";
    return false;
}

bool normalizeWritableEscapeSummaries(
    LibraryMetadata& metadata,
    std::string& error)
{
    for (LibraryMetadataMember& member : metadata.members)
    {
        if (!normalizeWritableEscapeSummary(member, "member", error)) return false;
    }
    for (LibraryMetadataNamespaceFunction& function : metadata.namespaceFunctions)
    {
        if (!normalizeWritableEscapeSummary(
                function, "namespace-function", error))
        {
            return false;
        }
    }
    return true;
}

bool validateWritableMetadataTarget(
    const LibraryMetadata& metadata,
    std::string& error)
{
    // EL OBJETIVO SE VALIDA, NO SE SUSTITUYE. Escribir la constante por encima de
    // lo que el modelo diga es exactamente lo que hacia el encabezado con el
    // minor: dos fuentes de verdad para el mismo campo. Y aqui seria peor, porque
    // esta funcion existe para MEDIR round-trips -- un campo que se sobrescribe
    // en silencio hace que la prueba diga "el objetivo sobrevivio" cuando lo que
    // sobrevivio es la constante.
    //
    // NativeSharp 2.0 solo tiene un objetivo, asi que lo unico correcto es
    // rechazar cualquier otro con su numero delante, y no producir un fichero que
    // el lector rechazaria despues.
    if (metadata.targetId == kWindowsX64Target &&
        metadata.pointerSize == kPointerSize &&
        metadata.pointerAlignment == kPointerAlignment)
    {
        return true;
    }
    error = "cannot write NativeSharp metadata for target " +
        std::to_string(metadata.targetId) + " with pointer " +
        std::to_string(metadata.pointerSize) + "/" +
        std::to_string(metadata.pointerAlignment) +
        "; NativeSharp 2.0 targets Windows x64 only";
    return false;
}

bool prepareWritableMetadata(
    const LibraryMetadata& model,
    LibraryMetadata& metadata,
    std::string& error)
{
    const std::array<LibraryMetadata, 1> fragments = {model};
    LibraryMetadataMerger merger;
    if (!merger.merge(fragments, metadata, error)) return false;
    // A model's content determines its representation, not its historical labels.
    metadata.formatMajor = kFormatMajor;
    metadata.formatMinor = kFormatMinor;
    normalizeWritableDelegateContracts(metadata);
    if (!normalizeWritableEscapeSummaries(metadata, error)) return false;
    if (!validateWritableMetadataTarget(metadata, error)) return false;
    return verifyFrozenAbiContract(metadata, error);
}

bool validateWritableSourceImports(
    LibraryMetadata& metadata,
    std::string& error)
{
    for (const auto& generic : metadata.genericTemplates)
    {
        for (const auto& directive : generic.sourceImports)
        {
            const bool enclosingScope = directive.namespaceName.empty() ||
                directive.namespaceName == generic.namespaceName ||
                (generic.namespaceName.starts_with(directive.namespaceName) &&
                 generic.namespaceName.size() > directive.namespaceName.size() &&
                 generic.namespaceName[directive.namespaceName.size()] == '.');
            if (directive.target.empty() || !enclosingScope || directive.importKind > 5U ||
                (directive.importKind == 0U && directive.reexport))
            {
                error = "invalid template source import or lexical scope";
                return false;
            }
        }
    }
    return true;
}

bool writeMetadataFile(
    LibraryMetadata& metadata,
    const std::filesystem::path& destination,
    LibraryMetadata* writtenModel,
    std::vector<std::uint8_t>* writtenBytes,
    std::string& error)
{
    metadata.abiFingerprint = basic::sha256(encodeAbiProjection(metadata));
    const std::vector<std::uint8_t> payload =
        encodePayload(metadata);
    if (payload.size() > kMaximumPayloadSize)
    {
        error = "NativeSharp metadata payload exceeds 128 MiB format limit";
        return false;
    }
    const std::array<std::uint8_t, 32> payloadHash = basic::sha256(payload);
    ByteWriter file;
    file.raw(kMagic);
    // Reserved header bytes do not select the layout or describe a format edition.
    file.u32(0U);
    // El marcador de endianidad es una propiedad de la CODIFICACION, no del
    // modelo: no hay campo que pueda decir otra cosa, y por eso este si es una
    // constante. Los tres siguientes salen del modelo, ya validado arriba.
    file.u32(kEndianMarker);
    file.u32(metadata.targetId);
    file.u32(metadata.pointerSize);
    file.u32(metadata.pointerAlignment);
    file.u64(static_cast<std::uint64_t>(payload.size()));
    file.raw(metadata.abiFingerprint);
    file.raw(payloadHash);
    file.raw(payload);
    if (!writeBytesAtomically(destination, file.bytes(), error)) return false;
    if (writtenBytes != nullptr) *writtenBytes = file.takeBytes();
    if (writtenModel != nullptr) *writtenModel = std::move(metadata);
    return true;
}

bool readMetadataBytes(
    const std::filesystem::path& source,
    std::vector<std::uint8_t>& bytes,
    std::string& error)
{
    std::ifstream stream(source, std::ios::binary | std::ios::ate);
    if (!stream)
    {
        error = "cannot open NativeSharp metadata '" + source.string() + "'";
        return false;
    }
    const std::streampos end = stream.tellg();
    if (end < 0 || static_cast<std::uint64_t>(end) > kHeaderSize + kMaximumPayloadSize)
    {
        error = "NativeSharp metadata file has invalid size";
        return false;
    }
    bytes.resize(static_cast<std::size_t>(end));
    stream.seekg(0, std::ios::beg);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream && !bytes.empty())
    {
        error = "failed while reading NativeSharp metadata";
        return false;
    }
    return true;
}

bool readMetadataHeader(
    ByteReader& reader,
    std::uint32_t& targetId,
    std::uint32_t& pointerSize,
    std::uint32_t& pointerAlignment,
    std::uint64_t& payloadSize,
    std::array<std::uint8_t, 32>& abiFingerprint,
    std::array<std::uint8_t, 32>& payloadHash,
    std::string& error)
{
    std::array<std::uint8_t, 8> magic{};
    if (!reader.raw(magic) || magic != kMagic)
    {
        error = "invalid NativeSharp metadata magic";
        return false;
    }
    std::uint32_t reserved = 0;
    std::uint32_t endianMarker = 0;
    if (!reader.u32(reserved) || !reader.u32(endianMarker) ||
        !reader.u32(targetId) || !reader.u32(pointerSize) || !reader.u32(pointerAlignment) ||
        !reader.u64(payloadSize) || !reader.raw(abiFingerprint) || !reader.raw(payloadHash))
    {
        error = "truncated NativeSharp metadata header";
        return false;
    }
    if (endianMarker == kEndianMarker) return true;
    error = "NativeSharp metadata endian marker mismatch";
    return false;
}

void initializeDecodedMetadata(
    LibraryMetadata& decoded,
    std::uint32_t targetId,
    std::uint32_t pointerSize,
    std::uint32_t pointerAlignment,
    const std::array<std::uint8_t, 32>& abiFingerprint)
{
    decoded.targetId = targetId;
    decoded.pointerSize = pointerSize;
    decoded.pointerAlignment = pointerAlignment;
    decoded.abiFingerprint = abiFingerprint;
}

bool authenticateDecodedMetadata(
    std::span<const std::uint8_t> payload,
    LibraryMetadata& decoded,
    std::string& error)
{
    if (!decodePayload(payload, decoded, error)) return false;
    const std::array<std::uint8_t, 32> recomputedAbi =
        basic::sha256(encodeAbiProjection(decoded));
    if (sameFingerprint(recomputedAbi, decoded.abiFingerprint)) return true;
    error = "NativeSharp metadata ABI fingerprint mismatch";
    // EN: A library from another object ABI also fails here once the projection changed; name the cause.
    // ES: Una biblioteca de otro object ABI también falla aquí si la proyección cambió; se nombra la causa.
    if (decoded.objectAbiVersion != kObjectAbiVersion)
        error += "; " + objectAbiMismatchMessage(decoded.objectAbiVersion);
    return false;
}

void normalizeDecodedMetadata(LibraryMetadata& decoded)
{
    // Normalize lookup order only after authenticating the ABI projection in the exact order
    // encoded by the producer. Older compatible producers appended synthesized constructors
    // after their sorted member table; sorting before the fingerprint check changes their
    // historical projection and incorrectly rejects otherwise valid NSMETA files.
    std::stable_sort(decoded.types.begin(), decoded.types.end(), [](const auto& left, const auto& right) {
        return left.identityKey < right.identityKey;
    });
    std::stable_sort(decoded.members.begin(), decoded.members.end(), [](const auto& left, const auto& right) {
        return left.declaringTypeKey < right.declaringTypeKey;
    });
    decoded.payloadDecodedByCurrentReader = true;
}

}
