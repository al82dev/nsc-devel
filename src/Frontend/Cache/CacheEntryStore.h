#pragma once

#include "CacheMetadata.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace nativesharp::frontend::internal
{
class CacheEntryStore final
{
public:
    CacheEntryStore(
        std::atomic<unsigned long long>& temporaryOrdinal,
        std::string_view metadataHeader,
        std::string_view metadataLabel,
        std::string_view publicationLabel,
        bool writeThrough,
        bool extendedWindowsPaths) noexcept;

    static bool validKey(std::string_view key) noexcept;

    std::filesystem::path temporarySibling(const std::filesystem::path& destination) const;
    bool replaceFileAtomically(
        const std::filesystem::path& temporary,
        const std::filesystem::path& destination,
        std::string& error) const;
    bool readMetadata(const std::filesystem::path& path, CacheMetadata& metadata) const;
    bool writeMetadataAtomically(
        const std::filesystem::path& destination,
        std::string_view key,
        std::string_view digest,
        std::uintmax_t size,
        std::string& error) const;

private:
    std::filesystem::path filesystemPath(const std::filesystem::path& path) const;

    std::atomic<unsigned long long>& temporaryOrdinal_;
    std::string_view metadataHeader_;
    std::string_view metadataLabel_;
    std::string_view publicationLabel_;
    bool writeThrough_;
    bool extendedWindowsPaths_;
};
}
