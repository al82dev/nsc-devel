#include "SnapshotTypeEligibility.h"

namespace nativesharp::frontend
{
const std::string* precompiledSnapshotSection(
    const PrecompiledSnapshotUnit& snapshot,
    std::string_view model) noexcept
{
    for (const compilation::PrecompiledSnapshotSection& section : snapshot.snapshot.sections)
    {
        if (section.model == model) return &section.payload;
    }
    return nullptr;
}

bool typePartsInFile(const NamedTypeDeclaration& type, FileId fileId) noexcept
{
    if (type.parts.empty()) return false;
    for (const NamedTypeDeclarationPart& part : type.parts)
    {
        if (part.fileId != fileId) return false;
    }
    return true;
}

const PrecompiledSnapshotUnit* snapshotOwningType(
    const NamedTypeDeclaration& type,
    std::span<const PrecompiledSnapshotUnit> snapshots) noexcept
{
    if (type.parts.empty()) return nullptr;
    for (const PrecompiledSnapshotUnit& snapshot : snapshots)
    {
        if (typePartsInFile(type, snapshot.fileId)) return &snapshot;
    }
    return nullptr;
}
}
