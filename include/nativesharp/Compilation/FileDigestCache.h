#pragma once

#include "nativesharp/Basic/Sha256.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace nativesharp::compilation
{
// Remembers the SHA-256 of input files between compilations. An entry is keyed by the canonical
// path, the size and the last write time of the file, the same evidence a build system uses to
// decide that a file did not change; a file whose size or write time changed is hashed again.
// Without a root every request hashes the file.
class FileDigestCache final
{
public:
    void setRoot(std::filesystem::path root);
    bool enabled() const noexcept;

    // Returns the digest and the size of `path`. Records the digest of a file hashed now.
    bool digest(
        const std::filesystem::path& path,
        basic::Sha256Digest& digest,
        std::uintmax_t& size,
        std::string& error) const;

private:
    std::filesystem::path root_;
};
} // namespace nativesharp::compilation
