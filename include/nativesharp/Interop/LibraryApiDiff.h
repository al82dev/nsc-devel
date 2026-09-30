#pragma once

#include "nativesharp/Interop/LibraryMetadata.h"

#include <string>
#include <vector>

namespace nativesharp::interop
{
enum class LibraryApiChangeImpact
{
    Additive,
    SourceBreaking,
    AbiBreaking
};

struct LibraryApiChange final
{
    LibraryApiChangeImpact impact = LibraryApiChangeImpact::Additive;
    std::string domain;
    std::string identity;
    std::string detail;
};

struct LibraryApiDiffReport final
{
    bool sourceCompatible = true;
    bool abiCompatible = true;
    std::string oldSourceFingerprint;
    std::string newSourceFingerprint;
    std::string oldAbiSurfaceFingerprint;
    std::string newAbiSurfaceFingerprint;
    std::vector<LibraryApiChange> changes;
};

class LibraryApiDiffer final
{
public:
    static LibraryApiDiffReport compare(
        const LibraryMetadata& oldMetadata,
        const LibraryMetadata& newMetadata);

    static std::string sourceFingerprint(const LibraryMetadata& metadata);
    static std::string abiSurfaceFingerprint(const LibraryMetadata& metadata);
    static const char* impactName(LibraryApiChangeImpact impact) noexcept;
    static std::string classification(const LibraryApiDiffReport& report);
};
}
