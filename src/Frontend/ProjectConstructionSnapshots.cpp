#include "nativesharp/Frontend/ProjectConstructionModel.h"

#include "Snapshot/ModelReferenceSnapshotCodec.h"
#include "Snapshot/SnapshotTypeEligibility.h"

#include <string>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
constexpr std::string_view kConstructionSection = "construction";
}

void ProjectConstructionModel::setPrecompiledSnapshots(std::span<const PrecompiledSnapshotUnit> snapshots) noexcept
{
    precompiledSnapshots_ = snapshots;
}

std::size_t ProjectConstructionModel::installedSnapshotClassCount() const noexcept
{
    return installedSnapshotClasses_;
}

void ProjectConstructionModel::setPrecompiledSnapshotCaptureFile(FileId fileId) noexcept
{
    snapshotCaptureFile_ = fileId;
}

// EN: Section format: u64 binding count, then per class its identity key, the resolution kind and
//     the optional direct-base and entry constructors. An entry whose references do not all
//     resolve, or whose class has a part outside a snapshot unit, is skipped and computed.
// ES: Formato de la sección: u64 con el número de bindings, luego por clase su llave de identidad,
//     el tipo de resolución y los constructores opcionales de base directa y de entrada. Una entrada
//     cuyas referencias no resuelven todas, o cuya clase tiene una parte fuera de una unidad con
//     snapshot, se omite y se calcula.
void ProjectConstructionModel::readSnapshotConstruction()
{
    for (const PrecompiledSnapshotUnit& snapshot : precompiledSnapshots_)
    {
        const std::string* const payload = precompiledSnapshotSection(snapshot, kConstructionSection);
        if (payload == nullptr) continue;
        PrecompiledSnapshotReader reader(*payload, snapshot.index);
        const std::uint64_t count = reader.u64();
        for (std::uint64_t entry = 0; entry < count && !reader.failed(); ++entry)
            readSnapshotImplicitDefault(reader);
    }
}

bool ProjectConstructionModel::readSnapshotImplicitDefault(PrecompiledSnapshotReader& reader)
{
    const std::optional<std::size_t> typeOrdinal = readTypeReference(reader, *nameLookup_);
    ImplicitDefaultConstructorBinding binding;
    binding.resolution = static_cast<ImplicitDefaultConstructorResolutionKind>(reader.byte());
    bool resolved =
        readOptionalMemberReference(reader, *nameLookup_, *memberModel_, binding.directBaseConstructorMemberOrdinal);
    resolved =
        readOptionalMemberReference(reader, *nameLookup_, *memberModel_, binding.entryConstructorMemberOrdinal) &&
        resolved;
    if (!resolved || !typeOrdinal.has_value() || reader.failed() ||
        snapshotOwningType(nameLookup_->namedTypes()[*typeOrdinal], precompiledSnapshots_) == nullptr)
        return false;
    binding.typeOrdinal = *typeOrdinal;
    binding.baseTypeOrdinal = inheritanceModel_->baseTypeOrdinal(*typeOrdinal);
    snapshotImplicitDefaults_.insert_or_assign(*typeOrdinal, std::move(binding));
    return true;
}

bool ProjectConstructionModel::installSnapshotImplicitDefault(std::size_t typeOrdinal)
{
    const auto found = snapshotImplicitDefaults_.find(typeOrdinal);
    if (found == snapshotImplicitDefaults_.end()) return false;
    implicitDefaultBindingByType_[typeOrdinal] = implicitDefaultConstructorBindings_.size();
    implicitDefaultConstructorBindings_.push_back(std::move(found->second));
    snapshotImplicitDefaults_.erase(found);
    ++installedSnapshotClasses_;
    return true;
}

bool ProjectConstructionModel::writePrecompiledSnapshotConstruction(std::string& payload, std::string& error) const
{
    if (!snapshotCaptureFile_.has_value() || !built_)
    {
        error = "the API unit classes were not captured";
        return false;
    }
    PrecompiledSyntaxNodeIndex noSyntax;
    PrecompiledSnapshotWriter entries(noSyntax);
    bool written = true;
    std::uint64_t count = 0;
    for (const ImplicitDefaultConstructorBinding& binding : implicitDefaultConstructorBindings_)
    {
        if (!typePartsInFile(nameLookup_->namedTypes()[binding.typeOrdinal], *snapshotCaptureFile_)) continue;
        writeTypeReference(entries, *nameLookup_, binding.typeOrdinal, written);
        entries.byte(static_cast<std::uint8_t>(binding.resolution));
        writeOptionalMemberReference(
            entries, *nameLookup_, *memberModel_, binding.directBaseConstructorMemberOrdinal, written);
        writeOptionalMemberReference(
            entries, *nameLookup_, *memberModel_, binding.entryConstructorMemberOrdinal, written);
        ++count;
    }
    if (!written || entries.failed())
    {
        error = "a construction entry of the API unit cannot be written by identity";
        return false;
    }
    PrecompiledSnapshotWriter header(noSyntax);
    header.u64(count);
    payload = header.take() + entries.take();
    return true;
}
}
