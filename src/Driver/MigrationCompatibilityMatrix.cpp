#include "nativesharp/Driver/MigrationCompatibilityMatrix.h"

#include <sstream>

namespace nativesharp::driver
{
MigrationCompatibilityMatrix buildMigrationCompatibilityMatrix(
    const interop::LibraryApiDiffReport& report,
    std::string oldMetadataAbiFingerprint,
    std::string newMetadataAbiFingerprint)
{
    MigrationCompatibilityMatrix matrix;
    matrix.oldSourceFingerprint = report.oldSourceFingerprint;
    matrix.newSourceFingerprint = report.newSourceFingerprint;
    matrix.oldAbiSurfaceFingerprint = report.oldAbiSurfaceFingerprint;
    matrix.newAbiSurfaceFingerprint = report.newAbiSurfaceFingerprint;
    matrix.oldMetadataAbiFingerprint = std::move(oldMetadataAbiFingerprint);
    matrix.newMetadataAbiFingerprint = std::move(newMetadataAbiFingerprint);
    matrix.sourceCompatible = report.sourceCompatible;
    matrix.abiCompatible = report.abiCompatible;
    matrix.changes.reserve(report.changes.size());
    for (const interop::LibraryApiChange& change : report.changes)
    {
        MigrationCompatibilityEntry entry;
        entry.domain = change.domain;
        entry.identity = change.identity;
        entry.detail = change.detail;
        switch (change.impact)
        {
            case interop::LibraryApiChangeImpact::Additive:
                entry.sourceClassification = "source-compatible";
                entry.abiClassification = "abi-neutral";
                break;
            case interop::LibraryApiChangeImpact::SourceBreaking:
                entry.sourceClassification = "source-breaking";
                entry.abiClassification = "abi-neutral";
                entry.requiresConsumerRebuild = true;
                break;
            case interop::LibraryApiChangeImpact::AbiBreaking:
                entry.sourceClassification = "source-compatible";
                entry.abiClassification = "abi-breaking";
                entry.requiresConsumerRebuild = true;
                break;
        }
        matrix.changes.push_back(std::move(entry));
    }
    return matrix;
}

std::string renderMigrationCompatibilityMatrix(
    const MigrationCompatibilityMatrix& matrix)
{
    std::ostringstream output;
    output << "NSMIGRATION-MATRIX 1\n"
           << "old-source-fingerprint=" << matrix.oldSourceFingerprint << '\n'
           << "new-source-fingerprint=" << matrix.newSourceFingerprint << '\n'
           << "old-abi-surface-fingerprint=" << matrix.oldAbiSurfaceFingerprint << '\n'
           << "new-abi-surface-fingerprint=" << matrix.newAbiSurfaceFingerprint << '\n'
           << "old-metadata-abi-fingerprint=" << matrix.oldMetadataAbiFingerprint << '\n'
           << "new-metadata-abi-fingerprint=" << matrix.newMetadataAbiFingerprint << '\n'
           << "source-compatible=" << (matrix.sourceCompatible ? "yes" : "no") << '\n'
           << "abi-compatible=" << (matrix.abiCompatible ? "yes" : "no") << '\n'
           << "changes=" << matrix.changes.size() << '\n';
    for (const MigrationCompatibilityEntry& change : matrix.changes)
    {
        output << "change domain=" << change.domain
               << " identity=" << change.identity
               << " source=" << change.sourceClassification
               << " abi=" << change.abiClassification
               << " consumer-rebuild=" << (change.requiresConsumerRebuild ? "yes" : "no")
               << " detail=" << change.detail << '\n';
    }
    return output.str();
}
}
