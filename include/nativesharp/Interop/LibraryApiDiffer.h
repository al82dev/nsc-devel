#pragma once

#include "nativesharp/Interop/LibraryApiChangeImpact.h"
#include "nativesharp/Interop/LibraryApiDiffReport.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include <string>

namespace nativesharp::interop
{
class LibraryApiDiffer final
{
public:
    static LibraryApiDiffReport compare(
        const LibraryMetadata& oldMetadata,
        const LibraryMetadata& newMetadata);

    static std::string sourceFingerprint(const LibraryMetadata& metadata);
    static std::string abiSurfaceFingerprint(const LibraryMetadata& metadata);
    // Stable, length-delimited semantic projections consumed by API tooling. They intentionally
    // omit source trivia and private declarations; hashes above are SHA-256 of these exact bytes.
    static std::string sourceSurfaceManifest(const LibraryMetadata& metadata);
    static std::string abiSurfaceManifest(const LibraryMetadata& metadata);
    static const char* impactName(LibraryApiChangeImpact impact) noexcept;
    static std::string classification(const LibraryApiDiffReport& report);
};
}
