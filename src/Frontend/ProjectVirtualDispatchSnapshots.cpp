#include "nativesharp/Frontend/ProjectVirtualDispatchModel.h"

#include "Snapshot/ModelReferenceSnapshotCodec.h"
#include "Snapshot/SnapshotTypeEligibility.h"

#include <string>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
constexpr std::string_view kDispatchSection = "dispatch";
}

void ProjectVirtualDispatchModel::setPrecompiledSnapshots(std::span<const PrecompiledSnapshotUnit> snapshots) noexcept
{
    precompiledSnapshots_ = snapshots;
}

std::size_t ProjectVirtualDispatchModel::installedSnapshotClassCount() const noexcept
{
    return installedSnapshotClasses_;
}

void ProjectVirtualDispatchModel::setPrecompiledSnapshotCaptureFile(FileId fileId) noexcept
{
    snapshotCaptureFile_ = fileId;
}

// EN: Section format: u64 class count, then per class its identity key, its slot members, its slot
//     abstract flags and the virtual methods its own members registered (member, slot, root member,
//     optional overridden member). An entry whose references do not all resolve, or whose class has
//     a part outside a snapshot unit, is skipped and computed.
// ES: Formato de la sección: u64 con el número de clases, luego por clase su llave de identidad, los
//     miembros de sus slots, las marcas abstractas de sus slots y los métodos virtuales que
//     registraron sus propios miembros (miembro, slot, miembro raíz, miembro sobrescrito opcional).
//     Una entrada cuyas referencias no resuelven todas, o cuya clase tiene una parte fuera de una
//     unidad con snapshot, se omite y se calcula.
void ProjectVirtualDispatchModel::readSnapshotDispatch()
{
    for (const PrecompiledSnapshotUnit& snapshot : precompiledSnapshots_)
    {
        const std::string* const payload = precompiledSnapshotSection(snapshot, kDispatchSection);
        if (payload == nullptr) continue;
        PrecompiledSnapshotReader reader(*payload, snapshot.index);
        const std::uint64_t count = reader.u64();
        for (std::uint64_t entry = 0; entry < count && !reader.failed(); ++entry) readSnapshotClassTable(reader);
    }
}

bool ProjectVirtualDispatchModel::readSnapshotClassTable(PrecompiledSnapshotReader& reader)
{
    const std::optional<std::size_t> typeOrdinal = readTypeReference(reader, *nameLookup_);
    ClassVirtualTableSnapshotEntry entry;
    bool resolved = readMemberReferences(reader, *nameLookup_, *memberModel_, entry.slotMemberOrdinals);
    const std::uint32_t abstractCount = reader.u32();
    for (std::uint32_t index = 0; index < abstractCount && !reader.failed(); ++index)
        entry.slotAbstract.push_back(reader.byte() != 0U);
    const std::uint32_t methodCount = reader.u32();
    for (std::uint32_t index = 0; index < methodCount && !reader.failed(); ++index)
    {
        VirtualMethodInfo info;
        const std::optional<std::size_t> member = readMemberReference(reader, *nameLookup_, *memberModel_);
        info.slotOrdinal = static_cast<std::size_t>(reader.u64());
        const std::optional<std::size_t> root = readMemberReference(reader, *nameLookup_, *memberModel_);
        resolved = readOptionalMemberReference(reader, *nameLookup_, *memberModel_, info.overriddenMemberOrdinal) &&
            member.has_value() && root.has_value() && resolved;
        info.memberOrdinal = member.value_or(0);
        info.rootVirtualMemberOrdinal = root.value_or(0);
        entry.methods.push_back(info);
    }
    if (!resolved || !typeOrdinal.has_value() || reader.failed() ||
        snapshotOwningType(nameLookup_->namedTypes()[*typeOrdinal], precompiledSnapshots_) == nullptr)
        return false;
    snapshotTables_.insert_or_assign(*typeOrdinal, std::move(entry));
    return true;
}

bool ProjectVirtualDispatchModel::installSnapshotClassTable(ClassVirtualTable& table)
{
    const auto found = snapshotTables_.find(table.typeOrdinal);
    if (found == snapshotTables_.end()) return false;
    table.slotMemberOrdinals = std::move(found->second.slotMemberOrdinals);
    table.slotAbstract = std::move(found->second.slotAbstract);
    for (const VirtualMethodInfo& info : found->second.methods)
    {
        virtualInfoByMember_[info.memberOrdinal] = virtualMethods_.size();
        virtualMethods_.push_back(info);
    }
    snapshotTables_.erase(found);
    ++installedSnapshotClasses_;
    return true;
}

void ProjectVirtualDispatchModel::captureSnapshotClassTable(std::size_t typeOrdinal, std::size_t methodsBegin)
{
    if (!snapshotCaptureFile_.has_value() ||
        !typePartsInFile(nameLookup_->namedTypes()[typeOrdinal], *snapshotCaptureFile_))
        return;
    capturedMethodRanges_.insert_or_assign(typeOrdinal, std::make_pair(methodsBegin, virtualMethods_.size()));
}

bool ProjectVirtualDispatchModel::writePrecompiledSnapshotDispatch(std::string& payload, std::string& error) const
{
    if (!snapshotCaptureFile_.has_value() || !built_)
    {
        error = "the API unit classes were not captured";
        return false;
    }
    PrecompiledSyntaxNodeIndex noSyntax;
    PrecompiledSnapshotWriter writer(noSyntax);
    bool written = true;
    writer.u64(capturedMethodRanges_.size());
    for (const ClassVirtualTable& table : classTables_)
    {
        const auto range = capturedMethodRanges_.find(table.typeOrdinal);
        if (range == capturedMethodRanges_.end()) continue;
        writeTypeReference(writer, *nameLookup_, table.typeOrdinal, written);
        writeMemberReferences(writer, *nameLookup_, *memberModel_, table.slotMemberOrdinals, written);
        writer.u32(static_cast<std::uint32_t>(table.slotAbstract.size()));
        for (const bool slotAbstract : table.slotAbstract) writer.byte(slotAbstract ? 1U : 0U);
        writer.u32(static_cast<std::uint32_t>(range->second.second - range->second.first));
        for (std::size_t index = range->second.first; index < range->second.second; ++index)
        {
            const VirtualMethodInfo& info = virtualMethods_[index];
            writeMemberReference(writer, *nameLookup_, *memberModel_, info.memberOrdinal, written);
            writer.u64(info.slotOrdinal);
            writeMemberReference(writer, *nameLookup_, *memberModel_, info.rootVirtualMemberOrdinal, written);
            writeOptionalMemberReference(writer, *nameLookup_, *memberModel_, info.overriddenMemberOrdinal, written);
        }
    }
    if (!written || writer.failed())
    {
        error = "a dispatch entry of the API unit cannot be written by identity";
        return false;
    }
    payload = writer.take();
    return true;
}
}
