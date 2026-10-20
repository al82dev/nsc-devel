#pragma once

#include "nativesharp/Driver/MigrationCompatibilityEntry.h"
#include "nativesharp/Interop/LibraryApiDiff.h"

#include <string>
#include <vector>

namespace nativesharp::driver
{
struct MigrationCompatibilityMatrix final
{
    std::string oldSourceFingerprint;
    std::string newSourceFingerprint;
    std::string oldAbiSurfaceFingerprint;
    std::string newAbiSurfaceFingerprint;
    std::string oldMetadataAbiFingerprint;
    std::string newMetadataAbiFingerprint;
    bool sourceCompatible = true;
    bool abiCompatible = true;
    std::vector<MigrationCompatibilityEntry> changes;
};

MigrationCompatibilityMatrix buildMigrationCompatibilityMatrix(
    const interop::LibraryApiDiffReport& report,
    std::string oldMetadataAbiFingerprint,
    std::string newMetadataAbiFingerprint);

std::string renderMigrationCompatibilityMatrix(
    const MigrationCompatibilityMatrix& matrix);
}
