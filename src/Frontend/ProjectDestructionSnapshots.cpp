#include "nativesharp/Frontend/ProjectDestructionModel.h"

#include "Snapshot/ModelReferenceSnapshotCodec.h"
#include "Snapshot/SnapshotTypeEligibility.h"

#include <string>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
constexpr std::string_view kDestructionSection = "destruction";
}

void ProjectDestructionModel::setPrecompiledSnapshots(std::span<const PrecompiledSnapshotUnit> snapshots) noexcept
{
    precompiledSnapshots_ = snapshots;
}

std::size_t ProjectDestructionModel::installedSnapshotClassCount() const noexcept
{
    return installedSnapshotClasses_;
}

void ProjectDestructionModel::setPrecompiledSnapshotCaptureFile(FileId fileId) noexcept
{
    snapshotCaptureFile_ = fileId;
}

// EN: Section format: u64 class count, then per class its identity key, the five entry names, the
//     lifetime ABI major, the optional explicit destructor and the destructor chain. An entry whose
//     references do not all resolve, or whose class has a part outside a snapshot unit, is skipped
//     and computed.
// ES: Formato de la sección: u64 con el número de clases, luego por clase su llave de identidad, los
//     cinco nombres de entrada, el major de la ABI de lifetime, el destructor explícito opcional y
//     la cadena de destructores. Una entrada cuyas referencias no resuelven todas, o cuya clase
//     tiene una parte fuera de una unidad con snapshot, se omite y se calcula.
void ProjectDestructionModel::readSnapshotDestruction()
{
    for (const PrecompiledSnapshotUnit& snapshot : precompiledSnapshots_)
    {
        const std::string* const payload = precompiledSnapshotSection(snapshot, kDestructionSection);
        if (payload == nullptr) continue;
        PrecompiledSnapshotReader reader(*payload, snapshot.index);
        const std::uint64_t count = reader.u64();
        for (std::uint64_t entry = 0; entry < count && !reader.failed(); ++entry) readSnapshotPlan(reader);
    }
}

bool ProjectDestructionModel::readSnapshotPlan(PrecompiledSnapshotReader& reader)
{
    const std::optional<std::size_t> typeOrdinal = readTypeReference(reader, *nameLookup_);
    ClassDestructionPlan plan;
    plan.typeOrdinal = typeOrdinal.value_or(0);
    plan.destructionEntryName = reader.text();
    plan.objectDestructionEntryName = reader.text();
    plan.crossModuleCreateEntryName = reader.text();
    plan.crossModuleDestroyEntryName = reader.text();
    plan.crossModuleDeallocateEntryName = reader.text();
    plan.crossModuleLifetimeAbiMajor = static_cast<std::uint16_t>(reader.u32());
    bool resolved =
        readOptionalMemberReference(reader, *nameLookup_, *memberModel_, plan.explicitDestructorMemberOrdinal);
    resolved = readMemberReferences(reader, *nameLookup_, *memberModel_, plan.destructorMemberOrdinals) && resolved;
    if (!resolved || !typeOrdinal.has_value() || reader.failed() ||
        snapshotOwningType(nameLookup_->namedTypes()[*typeOrdinal], precompiledSnapshots_) == nullptr)
        return false;
    snapshotPlans_.insert_or_assign(*typeOrdinal, std::move(plan));
    return true;
}

// EN: Installs the plan of `typeOrdinal` after building its base plan, so plans are appended in
//     the same order as today. Returns false when there is no entry; `built` is buildClass's result.
// ES: Instala el plan de `typeOrdinal` después de construir el plan de su base, para que los planes
//     se agreguen en el mismo orden que hoy. Regresa false si no hay entrada; `built` es el
//     resultado de buildClass.
bool ProjectDestructionModel::installSnapshotPlan(
    std::size_t typeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool& built)
{
    const auto found = snapshotPlans_.find(typeOrdinal);
    if (found == snapshotPlans_.end()) return false;
    ClassDestructionPlan plan = std::move(found->second);
    snapshotPlans_.erase(found);
    const std::optional<std::size_t> baseTypeOrdinal = inheritanceModel_->baseTypeOrdinal(typeOrdinal);
    if (baseTypeOrdinal.has_value() && (!buildClass(*baseTypeOrdinal, diagnostics) ||
            planForType(*baseTypeOrdinal) == nullptr))
    {
        buildingType_[typeOrdinal] = false;
        built = false;
        return true;
    }
    planByType_[typeOrdinal] = plans_.size();
    plans_.push_back(std::move(plan));
    buildingType_[typeOrdinal] = false;
    ++installedSnapshotClasses_;
    built = true;
    return true;
}

bool ProjectDestructionModel::writePrecompiledSnapshotDestruction(std::string& payload, std::string& error) const
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
    for (const ClassDestructionPlan& plan : plans_)
    {
        if (!typePartsInFile(nameLookup_->namedTypes()[plan.typeOrdinal], *snapshotCaptureFile_)) continue;
        writeTypeReference(entries, *nameLookup_, plan.typeOrdinal, written);
        entries.text(plan.destructionEntryName);
        entries.text(plan.objectDestructionEntryName);
        entries.text(plan.crossModuleCreateEntryName);
        entries.text(plan.crossModuleDestroyEntryName);
        entries.text(plan.crossModuleDeallocateEntryName);
        entries.u32(plan.crossModuleLifetimeAbiMajor);
        writeOptionalMemberReference(
            entries, *nameLookup_, *memberModel_, plan.explicitDestructorMemberOrdinal, written);
        writeMemberReferences(entries, *nameLookup_, *memberModel_, plan.destructorMemberOrdinals, written);
        ++count;
    }
    if (!written || entries.failed())
    {
        error = "a destruction entry of the API unit cannot be written by identity";
        return false;
    }
    PrecompiledSnapshotWriter header(noSyntax);
    header.u64(count);
    payload = header.take() + entries.take();
    return true;
}
}
