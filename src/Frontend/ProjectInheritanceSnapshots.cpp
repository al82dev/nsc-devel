#include "nativesharp/Frontend/ProjectInheritanceModel.h"

#include "Snapshot/PrecompiledSnapshotReader.h"
#include "Snapshot/PrecompiledSnapshotWriter.h"
#include "Snapshot/SnapshotTypeEligibility.h"

#include <string>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
constexpr std::string_view kInheritanceSection = "inheritance";
constexpr std::string_view kNoBase = "";
}

// EN: Section format: u64 count, then per class its identity key and its base identity key (empty
//     for a class without base). A section that does not read whole installs nothing.
// ES: Formato de la sección: u64 con el número, luego por clase su llave de identidad y la de su
//     base (vacía para una clase sin base). Una sección que no se lee completa no instala nada.
void ProjectInheritanceModel::setPrecompiledSnapshots(std::span<const PrecompiledSnapshotUnit> snapshots)
{
    precompiledSnapshots_ = snapshots;
    for (const PrecompiledSnapshotUnit& snapshot : snapshots)
    {
        const std::string* const payload = precompiledSnapshotSection(snapshot, kInheritanceSection);
        if (payload == nullptr) continue;
        PrecompiledSnapshotReader reader(*payload, snapshot.index);
        std::unordered_map<std::string, std::string> bases;
        const std::uint64_t count = reader.u64();
        for (std::uint64_t entry = 0; entry < count && !reader.failed(); ++entry)
        {
            std::string type = reader.typeKey();
            bases.emplace(std::move(type), reader.typeKey());
        }
        if (reader.failed() || !reader.atEnd()) continue;
        snapshotBases_.merge(bases);
    }
}

std::size_t ProjectInheritanceModel::installedSnapshotClassCount() const noexcept
{
    return installedSnapshotClasses_;
}

void ProjectInheritanceModel::setPrecompiledSnapshotCaptureFile(FileId fileId) noexcept
{
    snapshotCaptureFile_ = fileId;
}

void ProjectInheritanceModel::captureSnapshotBase(const ClassInheritanceInfo& info)
{
    const NamedTypeDeclaration& type = nameLookup_->namedTypes()[info.typeOrdinal];
    if (!snapshotCaptureFile_.has_value() || !typePartsInFile(type, *snapshotCaptureFile_)) return;
    capturedBases_.emplace_back(
        type.identity.key(),
        info.baseTypeOrdinal.has_value() ? nameLookup_->namedTypes()[*info.baseTypeOrdinal].identity.key()
                                         : std::string(kNoBase));
}

bool ProjectInheritanceModel::writePrecompiledSnapshotInheritance(std::string& payload, std::string& error) const
{
    if (!snapshotCaptureFile_.has_value())
    {
        error = "the API unit classes were not captured";
        return false;
    }
    // EN: Entries hold identity keys only; no syntax is referenced.
    // ES: Las entradas solo llevan llaves de identidad; no se referencia sintaxis.
    PrecompiledSyntaxNodeIndex noSyntax;
    PrecompiledSnapshotWriter writer(noSyntax);
    writer.u64(capturedBases_.size());
    for (const std::pair<std::string, std::string>& entry : capturedBases_)
    {
        writer.typeKey(entry.first);
        writer.typeKey(entry.second);
    }
    payload = writer.take();
    return true;
}

// EN: Takes the base the producer resolved for this class. Fails (and the class is resolved as
//     today) when the class has a part outside a snapshot unit, has no entry, or its base identity
//     is not a type of this compilation.
// ES: Toma la base que el productor resolvió para esta clase. Falla (y la clase se resuelve como
//     hoy) si la clase tiene una parte fuera de una unidad con snapshot, no tiene entrada, o la
//     identidad de su base no es un tipo de esta compilación.
bool ProjectInheritanceModel::installSnapshotBase(ClassInheritanceInfo& info)
{
    const NamedTypeDeclaration& type = nameLookup_->namedTypes()[info.typeOrdinal];
    if (snapshotOwningType(type, precompiledSnapshots_) == nullptr) return false;
    const auto found = snapshotBases_.find(type.identity.key());
    if (found == snapshotBases_.end()) return false;
    std::optional<std::size_t> base;
    if (found->second != kNoBase)
    {
        base = nameLookup_->typeOrdinalForIdentity(found->second);
        if (!base.has_value()) return false;
    }
    info.baseTypeOrdinal = base;
    if (base.has_value()) info.baseTypeKey = typeKeyForOrdinal(*base);
    ++installedSnapshotClasses_;
    return true;
}
}
