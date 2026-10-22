#pragma once

#include "ByteReader.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace nativesharp::frontend
{
class ProjectCompilation;
}

namespace nativesharp::interop::metadata_detail
{
constexpr std::array<std::uint8_t, 8> kMagic = {'N', 'S', 'M', 'E', 'T', 'A', '1', 0};
constexpr std::uint16_t kFormatMajor = kMetadataFormatMajor;
constexpr std::uint16_t kFormatMinor = kMetadataFormatMinor;
constexpr std::uint32_t kEndianMarker = 0x01020304U;
constexpr std::uint32_t kWindowsX64Target = 1;
constexpr std::uint32_t kPointerSize = 8;
constexpr std::uint32_t kPointerAlignment = 8;
constexpr std::string_view kTargetTriple = "x86_64-pc-windows-msvc";
constexpr std::size_t kHeaderSize = 100;
constexpr std::uint64_t kMaximumPayloadSize = 128ULL * 1024ULL * 1024ULL;
constexpr std::uint32_t kMaximumRecordCount = 1'000'000U;
constexpr std::uint8_t kLegacySourceRevisionByte = 3U;
constexpr std::uint8_t kLegacyAbiProjectionSourceRevisionByte = 0U;

std::string foldedAscii(std::string value);
std::string metadataBaseTypeKey(std::string typeKey);
std::optional<std::string> metadataTypeModule(std::string_view writtenTypeKey);
std::optional<std::string> specializedTypeKey(
    const LibraryMetadataGenericSpecialization& specialization);
bool looksLikeCompilerSpecializedType(std::string_view typeKey);
bool readOwnership(
    ByteReader& reader,
    std::uint16_t formatMinor,
    LibraryMetadataOwnershipKind& ownership);

bool buildMetadataModel(
    const frontend::ProjectCompilation& compilation,
    LibraryMetadata& metadata,
    std::string& error);

std::vector<std::uint8_t> encodePayload(
    const LibraryMetadata& metadata,
    std::uint8_t legacySourceRevision,
    bool includeTraitImpls);
std::vector<std::uint8_t> encodeAbiProjection(const LibraryMetadata& metadata);
bool decodePayload(
    std::span<const std::uint8_t> payload,
    LibraryMetadata& metadata,
    std::string& error);

bool sameFingerprint(
    const std::array<std::uint8_t, 32>& left,
    const std::array<std::uint8_t, 32>& right) noexcept;
bool verifyFrozenAbiContract(const LibraryMetadata& metadata, std::string& error);
bool prepareWritableMetadata(
    const LibraryMetadata& model,
    LibraryMetadata& metadata,
    std::string& error);
bool applyWritableMetadataDowngrade(
    LibraryMetadata& metadata,
    std::string& error);
bool writeMetadataFile(
    LibraryMetadata& metadata,
    const std::filesystem::path& destination,
    LibraryMetadata* writtenModel,
    std::vector<std::uint8_t>* writtenBytes,
    std::string& error);
bool readMetadataBytes(
    const std::filesystem::path& source,
    std::vector<std::uint8_t>& bytes,
    std::string& error);
bool readMetadataHeader(
    ByteReader& reader,
    std::uint16_t& major,
    std::uint16_t& minor,
    std::uint32_t& targetId,
    std::uint32_t& pointerSize,
    std::uint32_t& pointerAlignment,
    std::uint64_t& payloadSize,
    std::array<std::uint8_t, 32>& abiFingerprint,
    std::array<std::uint8_t, 32>& payloadHash,
    std::string& error);
void initializeDecodedMetadata(
    LibraryMetadata& decoded,
    std::uint16_t major,
    std::uint16_t minor,
    std::uint32_t targetId,
    std::uint32_t pointerSize,
    std::uint32_t pointerAlignment,
    const std::array<std::uint8_t, 32>& abiFingerprint);
bool authenticateDecodedMetadata(
    std::span<const std::uint8_t> payload,
    LibraryMetadata& decoded,
    std::string& error);
void normalizeDecodedMetadata(LibraryMetadata& decoded);

bool verifyMetadataEnvelope(const LibraryMetadata& metadata, std::string& error);
bool verifyMetadataTypeDeclarations(const LibraryMetadata& metadata, std::string& error);
bool verifyMetadataReferenceAndDelegateContracts(
    const LibraryMetadata& metadata,
    std::string& error);
bool verifyMetadataDispatchContracts(const LibraryMetadata& metadata, std::string& error);
bool verifyGenericAndExternalTypeCatalogues(
    const LibraryMetadata& metadata,
    std::unordered_set<std::string>& declaredTypeKeys,
    std::unordered_set<std::string>& specializedTypeKeys,
    std::unordered_set<std::string>& externalTypeKeys,
    std::string& error);
bool verifyAllMetadataTypeReferences(
    const LibraryMetadata& metadata,
    const std::unordered_set<std::string>& declaredTypeKeys,
    const std::unordered_set<std::string>& specializedTypeKeys,
    const std::unordered_set<std::string>& externalTypeKeys,
    std::string& error);
bool verifyNamespaceAndExportCatalogues(
    const LibraryMetadata& metadata,
    std::string& error);
bool verifyBinaryClosureMetadata(const LibraryMetadata& metadata, std::string& error);
bool verifyVersionedMemberContracts(const LibraryMetadata& metadata, std::string& error);
bool verifyLifetimeMetadataContracts(const LibraryMetadata& metadata, std::string& error);
}
