#pragma once

#include "nativesharp/Compilation/LibraryMetadataRepositoryEntry.h"
#include "nativesharp/Compilation/VerifiedLibraryMetadataCache.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>

namespace nativesharp::compilation
{
class LibraryMetadataRepository final
{
public:
    using Decoder = std::function<bool(
        std::span<const std::uint8_t>,
        interop::LibraryMetadata&,
        std::string&)>;
    using Verifier = std::function<bool(
        const interop::LibraryMetadata&,
        std::string&)>;

    void setCacheRoot(std::filesystem::path root);
    LibraryMetadataRepositoryEntry& entryFor(const std::filesystem::path& path);
    const LibraryMetadataRepositoryEntry* load(
        const std::filesystem::path& path,
        std::string& error) const;
    std::shared_ptr<const interop::LibraryMetadata> decode(
        const std::filesystem::path& path,
        const Decoder& decoder,
        std::string& error) const;
    bool verify(
        const std::filesystem::path& path,
        const Verifier& verifier,
        std::string& error) const;
    const LibraryMetadataRepositoryEntry* find(const std::filesystem::path& path) const;
    std::size_t size() const noexcept;

private:
    static std::filesystem::path canonicalPath(const std::filesystem::path& path);
    static std::string pathKey(const std::filesystem::path& path);

    VerifiedLibraryMetadataCache verifiedCache_;
    mutable std::unordered_map<std::string, std::unique_ptr<LibraryMetadataRepositoryEntry>> entries_;
};
}
