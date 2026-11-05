#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace nativesharp::interop
{
// Private link input. Names are resolved beside the metadata; digests identify contents.
struct LibraryMetadataCppBinaryDependency final
{
    std::string libraryFileName;
    std::uint64_t librarySize = 0;
    std::array<std::uint8_t, 32> libraryDigest{};
    std::string runtimeFileName;
    std::uint64_t runtimeSize = 0;
    std::array<std::uint8_t, 32> runtimeDigest{};
    bool operator==(const LibraryMetadataCppBinaryDependency&) const = default;
};
}
