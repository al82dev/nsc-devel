#include "nativesharp/Frontend/ProjectInterfaceModel.h"

#include "Snapshot/ModelReferenceSnapshotCodec.h"
#include "Snapshot/SnapshotTypeEligibility.h"

#include <string>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
constexpr std::string_view kInterfacesSection = "interfaces";
}

void ProjectInterfaceModel::setPrecompiledSnapshots(std::span<const PrecompiledSnapshotUnit> snapshots) noexcept
{
    precompiledSnapshots_ = snapshots;
}

std::size_t ProjectInterfaceModel::installedSnapshotInterfaceCount() const noexcept
{
    return installedSnapshotInterfaces_;
}

std::size_t ProjectInterfaceModel::installedSnapshotImplementingTypeCount() const noexcept
{
    return installedSnapshotImplementingTypes_;
}

void ProjectInterfaceModel::setPrecompiledSnapshotCaptureFile(FileId fileId) noexcept
{
    snapshotCaptureFile_ = fileId;
}

// EN: Section format: u64 interface count and interface entries, then u64 implementing-type count
//     and implementing-type entries. An entry whose references do not all resolve, or whose type
//     has a part outside a snapshot unit, is skipped and computed; a section that does not read
//     whole stops installing from that point.
// ES: Formato de la sección: u64 con el número de interfaces y sus entradas, luego u64 con el
//     número de tipos implementadores y sus entradas. Una entrada cuyas referencias no resuelven
//     todas, o cuyo tipo tiene una parte fuera de una unidad con snapshot, se omite y se calcula;
//     una sección que no se lee completa deja de instalar desde ese punto.
void ProjectInterfaceModel::installSnapshotInterfaces(std::vector<VisitState>& states)
{
    snapshotInstalledInterfaces_.assign(nameLookup_->namedTypes().size(), false);
    for (const PrecompiledSnapshotUnit& snapshot : precompiledSnapshots_)
    {
        const std::string* const payload = precompiledSnapshotSection(snapshot, kInterfacesSection);
        if (payload == nullptr) continue;
        PrecompiledSnapshotReader reader(*payload, snapshot.index);
        const std::uint64_t interfaceCount = reader.u64();
        for (std::uint64_t entry = 0; entry < interfaceCount && !reader.failed(); ++entry)
            installSnapshotInterface(reader);
        const std::uint64_t typeCount = reader.u64();
        for (std::uint64_t entry = 0; entry < typeCount && !reader.failed(); ++entry)
            readSnapshotImplementingType(reader);
    }
    for (std::size_t ordinal = 0; ordinal < snapshotInstalledInterfaces_.size(); ++ordinal)
    {
        if (snapshotInstalledInterfaces_[ordinal]) states[ordinal] = VisitState::Complete;
    }
}

bool ProjectInterfaceModel::installSnapshotInterface(PrecompiledSnapshotReader& reader)
{
    const std::optional<std::size_t> typeOrdinal = readTypeReference(reader, *nameLookup_);
    InterfaceTypeInfo read;
    bool resolved = readTypeReferences(reader, *nameLookup_, read.directBaseInterfaceOrdinals);
    resolved = readTypeReferences(reader, *nameLookup_, read.transitiveBaseInterfaceOrdinals) && resolved;
    resolved = readMemberReferences(reader, *nameLookup_, *memberModel_, read.effectiveMethodMemberOrdinals) &&
        resolved;
    resolved = readMemberReferences(reader, *nameLookup_, *memberModel_, read.requiredStaticMemberOrdinals) &&
        resolved;
    if (!resolved || !typeOrdinal.has_value() || reader.failed()) return false;
    const InterfaceTypeInfo* const existing = interfaceInfo(*typeOrdinal);
    if (existing == nullptr || snapshotInstalledInterfaces_[*typeOrdinal] ||
        snapshotOwningType(nameLookup_->namedTypes()[*typeOrdinal], precompiledSnapshots_) == nullptr)
        return false;
    InterfaceTypeInfo& info = interfaces_[*interfaceInfoByType_[*typeOrdinal]];
    info.directBaseInterfaceOrdinals = std::move(read.directBaseInterfaceOrdinals);
    info.transitiveBaseInterfaceOrdinals = std::move(read.transitiveBaseInterfaceOrdinals);
    info.effectiveMethodMemberOrdinals = std::move(read.effectiveMethodMemberOrdinals);
    info.requiredStaticMemberOrdinals = std::move(read.requiredStaticMemberOrdinals);
    snapshotInstalledInterfaces_[*typeOrdinal] = true;
    ++installedSnapshotInterfaces_;
    return true;
}

bool ProjectInterfaceModel::readSnapshotPlan(
    PrecompiledSnapshotReader& reader,
    std::size_t classOrdinal,
    ClassInterfaceSnapshotEntry& entry)
{
    ClassInterfaceTablePlan plan;
    plan.classTypeOrdinal = classOrdinal;
    const std::optional<std::size_t> interfaceOrdinal = readTypeReference(reader, *nameLookup_);
    plan.stableInterfaceTypeId = reader.u64();
    bool resolved = readMemberReferences(reader, *nameLookup_, *memberModel_, plan.interfaceMethodMemberOrdinals);
    const std::uint32_t implementationCount = reader.u32();
    for (std::uint32_t index = 0; index < implementationCount && !reader.failed(); ++index)
    {
        std::optional<std::size_t> implementation;
        resolved = readOptionalMemberReference(reader, *nameLookup_, *memberModel_, implementation) && resolved;
        plan.implementationMemberOrdinals.push_back(implementation);
    }
    if (!interfaceOrdinal.has_value() || !resolved || reader.failed()) return false;
    plan.interfaceTypeOrdinal = *interfaceOrdinal;
    entry.tablePlans.push_back(std::move(plan));
    return true;
}

bool ProjectInterfaceModel::readSnapshotImplementingType(PrecompiledSnapshotReader& reader)
{
    const std::optional<std::size_t> typeOrdinal = readTypeReference(reader, *nameLookup_);
    const std::size_t classOrdinal = typeOrdinal.value_or(nameLookup_->namedTypes().size());
    ClassInterfaceSnapshotEntry entry;
    bool resolved = readTypeReferences(reader, *nameLookup_, entry.implementedInterfaceOrdinals);
    const std::uint32_t witnessCount = reader.u32();
    for (std::uint32_t index = 0; index < witnessCount && !reader.failed(); ++index)
    {
        TraitOperatorWitness witness;
        witness.implementingTypeOrdinal = classOrdinal;
        const std::optional<std::size_t> trait = readTypeReference(reader, *nameLookup_);
        const std::optional<std::size_t> requirement = readMemberReference(reader, *nameLookup_, *memberModel_);
        const std::optional<std::size_t> implementation = readMemberReference(reader, *nameLookup_, *memberModel_);
        witness.langItem = reader.text();
        resolved = resolved && trait.has_value() && requirement.has_value() && implementation.has_value();
        witness.traitTypeOrdinal = trait.value_or(0);
        witness.requirementMemberOrdinal = requirement.value_or(0);
        witness.implementationMemberOrdinal = implementation.value_or(0);
        entry.operatorWitnesses.push_back(std::move(witness));
    }
    const std::uint32_t planCount = reader.u32();
    for (std::uint32_t index = 0; index < planCount && !reader.failed(); ++index)
        resolved = readSnapshotPlan(reader, classOrdinal, entry) && resolved;
    if (!resolved || !typeOrdinal.has_value() || reader.failed() ||
        snapshotOwningType(nameLookup_->namedTypes()[classOrdinal], precompiledSnapshots_) == nullptr)
        return false;
    snapshotImplementingTypes_.insert_or_assign(classOrdinal, std::move(entry));
    return true;
}

bool ProjectInterfaceModel::installSnapshotImplementingType(std::size_t classOrdinal)
{
    const auto found = snapshotImplementingTypes_.find(classOrdinal);
    if (found == snapshotImplementingTypes_.end()) return false;
    ClassInterfaceSnapshotEntry& entry = found->second;
    interfacesByClass_[classOrdinal] = std::move(entry.implementedInterfaceOrdinals);
    operatorWitnessesByType_[classOrdinal] = std::move(entry.operatorWitnesses);
    for (ClassInterfaceTablePlan& plan : entry.tablePlans)
    {
        tablePlanOrdinalsByClass_[classOrdinal].push_back(tablePlans_.size());
        tablePlans_.push_back(std::move(plan));
    }
    snapshotImplementingTypes_.erase(found);
    ++installedSnapshotImplementingTypes_;
    return true;
}

void ProjectInterfaceModel::writeSnapshotInterface(
    PrecompiledSnapshotWriter& writer,
    const InterfaceTypeInfo& info,
    bool& written) const
{
    writeTypeReference(writer, *nameLookup_, info.typeOrdinal, written);
    writeTypeReferences(writer, *nameLookup_, info.directBaseInterfaceOrdinals, written);
    writeTypeReferences(writer, *nameLookup_, info.transitiveBaseInterfaceOrdinals, written);
    writeMemberReferences(writer, *nameLookup_, *memberModel_, info.effectiveMethodMemberOrdinals, written);
    writeMemberReferences(writer, *nameLookup_, *memberModel_, info.requiredStaticMemberOrdinals, written);
}

void ProjectInterfaceModel::writeSnapshotImplementingType(
    PrecompiledSnapshotWriter& writer,
    std::size_t classOrdinal,
    bool& written) const
{
    writeTypeReference(writer, *nameLookup_, classOrdinal, written);
    writeTypeReferences(writer, *nameLookup_, interfacesByClass_[classOrdinal], written);
    writer.u32(static_cast<std::uint32_t>(operatorWitnessesByType_[classOrdinal].size()));
    for (const TraitOperatorWitness& witness : operatorWitnessesByType_[classOrdinal])
    {
        written = written && witness.implementingTypeOrdinal == classOrdinal;
        writeTypeReference(writer, *nameLookup_, witness.traitTypeOrdinal, written);
        writeMemberReference(writer, *nameLookup_, *memberModel_, witness.requirementMemberOrdinal, written);
        writeMemberReference(writer, *nameLookup_, *memberModel_, witness.implementationMemberOrdinal, written);
        writer.text(witness.langItem);
    }
    writer.u32(static_cast<std::uint32_t>(tablePlanOrdinalsByClass_[classOrdinal].size()));
    for (const std::size_t planOrdinal : tablePlanOrdinalsByClass_[classOrdinal])
    {
        const ClassInterfaceTablePlan& plan = tablePlans_[planOrdinal];
        written = written && plan.classTypeOrdinal == classOrdinal;
        writeTypeReference(writer, *nameLookup_, plan.interfaceTypeOrdinal, written);
        writer.u64(plan.stableInterfaceTypeId);
        writeMemberReferences(writer, *nameLookup_, *memberModel_, plan.interfaceMethodMemberOrdinals, written);
        writer.u32(static_cast<std::uint32_t>(plan.implementationMemberOrdinals.size()));
        for (const std::optional<std::size_t>& implementation : plan.implementationMemberOrdinals)
            writeOptionalMemberReference(writer, *nameLookup_, *memberModel_, implementation, written);
    }
}

bool ProjectInterfaceModel::writePrecompiledSnapshotInterfaces(std::string& payload, std::string& error) const
{
    if (!snapshotCaptureFile_.has_value() || !built_)
    {
        error = "the API unit interfaces were not captured";
        return false;
    }
    PrecompiledSyntaxNodeIndex noSyntax;
    PrecompiledSnapshotWriter interfaces(noSyntax);
    bool written = true;
    std::uint64_t interfaceCount = 0;
    for (const InterfaceTypeInfo& info : interfaces_)
    {
        if (!typePartsInFile(nameLookup_->namedTypes()[info.typeOrdinal], *snapshotCaptureFile_)) continue;
        writeSnapshotInterface(interfaces, info, written);
        ++interfaceCount;
    }
    PrecompiledSnapshotWriter types(noSyntax);
    std::uint64_t typeCount = 0;
    for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size(); ++ordinal)
    {
        const NamedTypeDeclaration& type = nameLookup_->namedTypes()[ordinal];
        if ((type.kind != NamedTypeDeclarationKind::Class && type.kind != NamedTypeDeclarationKind::Struct) ||
            !typePartsInFile(type, *snapshotCaptureFile_))
            continue;
        writeSnapshotImplementingType(types, ordinal, written);
        ++typeCount;
    }
    if (!written || interfaces.failed() || types.failed())
    {
        error = "an interface entry of the API unit cannot be written by identity";
        return false;
    }
    PrecompiledSnapshotWriter counts(noSyntax);
    counts.u64(interfaceCount);
    payload = counts.take() + interfaces.take();
    PrecompiledSnapshotWriter typeHeader(noSyntax);
    typeHeader.u64(typeCount);
    payload += typeHeader.take() + types.take();
    return true;
}
}
