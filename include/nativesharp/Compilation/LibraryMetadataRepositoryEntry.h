#pragma once

#include "nativesharp/Basic/Sha256.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::interop
{
struct LibraryMetadata;
}

namespace nativesharp::compilation
{
class LibraryMetadataRepository;

class LibraryMetadataRepositoryEntry final
{
public:
    explicit LibraryMetadataRepositoryEntry(std::filesystem::path path);

    bool load(std::string& error) const;
    const std::filesystem::path& path() const noexcept;
    std::span<const std::uint8_t> bytes() const noexcept;
    const basic::Sha256Digest& contentDigest() const noexcept;

private:
    friend class LibraryMetadataRepository;

    std::filesystem::path path_;
    mutable std::vector<std::uint8_t> bytes_;
    mutable basic::Sha256Digest contentDigest_{};
    mutable std::string loadError_;
    mutable std::shared_ptr<const interop::LibraryMetadata> decodedMetadata_;
    mutable std::string decodeError_;
    mutable std::string verificationError_;
    mutable bool loadAttempted_ = false;
    mutable bool decodeAttempted_ = false;
    mutable bool verificationAttempted_ = false;
    mutable bool verified_ = false;
};
}
