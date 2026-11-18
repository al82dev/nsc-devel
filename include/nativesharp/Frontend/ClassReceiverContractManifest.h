#pragma once

#include "nativesharp/Basic/Sha256.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
// This transport does not authenticate a producer and is not a NoEscape proof.
// Nothing in this file makes a decoded record available to semantic analysis.
enum class ReceiverContractKind : std::uint8_t { Constructor = 0, Destructor = 1 };

struct ReceiverContractRecord final
{
    ReceiverContractKind kind;
    std::string identity;
    bool operator==(const ReceiverContractRecord&) const = default;
};

struct ReceiverContractManifest final
{
    basic::Sha256Digest libraryDigest{};
    basic::Sha256Digest metadataDigest{};
    std::vector<ReceiverContractRecord> records;
    bool operator==(const ReceiverContractManifest&) const = default;
};

inline constexpr std::size_t kReceiverContractMaxRecords = 4096;
inline constexpr std::size_t kReceiverContractMaxIdentity = 1024;
inline constexpr std::size_t kReceiverContractMaxBytes = 1024 * 1024;
inline constexpr std::array<std::uint8_t, 8> kReceiverContractMagic =
    {'N', 'S', 'R', 'C', 1, 0, 0, 0};

inline bool receiverContractRecordLess(const ReceiverContractRecord& a, const ReceiverContractRecord& b)
{
    if (a.kind != b.kind) return a.kind < b.kind;
    return a.identity < b.identity;
}

inline bool validReceiverContractRecord(const ReceiverContractRecord& record)
{
    if (record.kind != ReceiverContractKind::Constructor && record.kind != ReceiverContractKind::Destructor)
        return false;
    if (record.identity.empty() || record.identity.size() > kReceiverContractMaxIdentity) return false;
    return std::ranges::all_of(record.identity, [](unsigned char c) { return c >= 0x21 && c <= 0x7e; });
}

// Encode only canonical, sorted, unique records; callers must establish their
// meaning separately. Reject oversized inputs before allocating the output.
inline std::optional<std::vector<std::uint8_t>> encodeReceiverContractManifest(
    const ReceiverContractManifest& manifest)
{
    if (manifest.records.size() > kReceiverContractMaxRecords) return std::nullopt;
    std::size_t size = kReceiverContractMagic.size() + 64 + 4;
    for (std::size_t i = 0; i < manifest.records.size(); ++i)
    {
        if (!validReceiverContractRecord(manifest.records[i]) ||
            (i != 0 && !receiverContractRecordLess(manifest.records[i - 1], manifest.records[i])))
            return std::nullopt;
        size += 3 + manifest.records[i].identity.size();
        if (size > kReceiverContractMaxBytes) return std::nullopt;
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(size);
    bytes.insert(bytes.end(), kReceiverContractMagic.begin(), kReceiverContractMagic.end());
    bytes.insert(bytes.end(), manifest.libraryDigest.begin(), manifest.libraryDigest.end());
    bytes.insert(bytes.end(), manifest.metadataDigest.begin(), manifest.metadataDigest.end());
    const auto count = static_cast<std::uint32_t>(manifest.records.size());
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<std::uint8_t>(count >> shift));
    for (const ReceiverContractRecord& record : manifest.records)
    {
        bytes.push_back(static_cast<std::uint8_t>(record.kind));
        const auto length = static_cast<std::uint16_t>(record.identity.size());
        bytes.push_back(static_cast<std::uint8_t>(length));
        bytes.push_back(static_cast<std::uint8_t>(length >> 8));
        bytes.insert(bytes.end(), record.identity.begin(), record.identity.end());
    }
    return bytes;
}

inline std::optional<ReceiverContractManifest> decodeReceiverContractManifest(
    std::span<const std::uint8_t> bytes)
{
    constexpr std::size_t headerSize = 8 + 64 + 4;
    if (bytes.size() < headerSize || bytes.size() > kReceiverContractMaxBytes ||
        !std::equal(kReceiverContractMagic.begin(), kReceiverContractMagic.end(), bytes.begin()))
        return std::nullopt;
    ReceiverContractManifest result;
    std::copy_n(bytes.begin() + 8, 32, result.libraryDigest.begin());
    std::copy_n(bytes.begin() + 40, 32, result.metadataDigest.begin());
    std::uint32_t count = 0;
    for (unsigned shift = 0; shift < 32; shift += 8)
        count |= static_cast<std::uint32_t>(bytes[72 + shift / 8]) << shift;
    if (count > kReceiverContractMaxRecords) return std::nullopt;
    std::size_t offset = headerSize;
    result.records.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        if (bytes.size() - offset < 3) return std::nullopt;
        const auto kind = static_cast<ReceiverContractKind>(bytes[offset]);
        const std::size_t length = static_cast<std::size_t>(bytes[offset + 1]) |
            (static_cast<std::size_t>(bytes[offset + 2]) << 8);
        offset += 3;
        if (length > bytes.size() - offset || length > kReceiverContractMaxIdentity)
            return std::nullopt;
        ReceiverContractRecord record{kind, std::string(bytes.begin() + offset, bytes.begin() + offset + length)};
        if (!validReceiverContractRecord(record) ||
            (!result.records.empty() && !receiverContractRecordLess(result.records.back(), record)))
            return std::nullopt;
        result.records.push_back(std::move(record));
        offset += length;
    }
    if (offset != bytes.size()) return std::nullopt;
    return result;
}

// A bounded read is observational until the caller also retains a product
// lock and carries the exact artifacts through linking.
inline std::optional<ReceiverContractManifest> readReceiverContractManifest(
    const std::filesystem::path& path)
{
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > kReceiverContractMaxBytes) return std::nullopt;
    std::ifstream input(path, std::ios::binary);
    if (!input) return std::nullopt;
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (input.gcount() != static_cast<std::streamsize>(bytes.size()) ||
        input.peek() != std::char_traits<char>::eof()) return std::nullopt;
    return decodeReceiverContractManifest(bytes);
}

// MatchedBytes is intentionally weaker than a trusted contract: it does not
// authenticate the producer, verify dependencies, or prevent later mutation.
enum class ReceiverManifestPairStatus { Unknown, MatchedBytes };

inline ReceiverManifestPairStatus verifyReceiverManifestPair(
    const std::optional<ReceiverContractManifest>& manifest,
    const std::filesystem::path& library, const std::filesystem::path& metadata)
{
    if (!manifest.has_value()) return ReceiverManifestPairStatus::Unknown;
    basic::Sha256Digest libraryDigest{}, metadataDigest{};
    std::uintmax_t size = 0;
    std::string error;
    if (!basic::sha256File(library, libraryDigest, size, error) ||
        !basic::sha256File(metadata, metadataDigest, size, error) ||
        libraryDigest != manifest->libraryDigest || metadataDigest != manifest->metadataDigest)
        return ReceiverManifestPairStatus::Unknown;
    return ReceiverManifestPairStatus::MatchedBytes;
}
}
