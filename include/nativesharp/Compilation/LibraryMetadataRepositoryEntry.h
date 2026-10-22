#pragma once

#include "nativesharp/Basic/Sha256.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::compilation
{
class LibraryMetadataRepositoryEntry final
{
public:
    explicit LibraryMetadataRepositoryEntry(std::filesystem::path path);

    bool load(std::string& error);
    const std::filesystem::path& path() const noexcept;
    std::span<const std::uint8_t> bytes() const noexcept;
    const basic::Sha256Digest& contentDigest() const noexcept;

private:
    std::filesystem::path path_;
    std::vector<std::uint8_t> bytes_;
    basic::Sha256Digest contentDigest_{};
    std::string loadError_;
    bool loadAttempted_ = false;
};
}
