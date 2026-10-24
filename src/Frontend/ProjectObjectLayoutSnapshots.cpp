#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"

#include "Snapshot/ModelReferenceSnapshotCodec.h"
#include "Snapshot/SnapshotCodecSupport.h"
#include "Snapshot/SnapshotTypeEligibility.h"

#include <string>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
constexpr std::string_view kLayoutSection = "layout";
}

void ProjectObjectLayoutModel::setPrecompiledSnapshots(std::span<const PrecompiledSnapshotUnit> snapshots) noexcept
{
    precompiledSnapshots_ = snapshots;
}

std::size_t ProjectObjectLayoutModel::installedSnapshotStructCount() const noexcept
{
    return installedSnapshotStructs_;
}

std::size_t ProjectObjectLayoutModel::installedSnapshotClassCount() const noexcept
{
    return installedSnapshotClasses_;
}

void ProjectObjectLayoutModel::setPrecompiledSnapshotCaptureFile(FileId fileId) noexcept
{
    snapshotCaptureFile_ = fileId;
}

void ProjectObjectLayoutModel::recordLayoutNeed(bool isClass, std::size_t typeOrdinal)
{
    if (!layoutNeedFrames_.empty()) layoutNeedFrames_.back().second.emplace_back(isClass, typeOrdinal);
}

void ProjectObjectLayoutModel::beginLayoutNeeds(std::size_t typeOrdinal)
{
    if (snapshotCaptureFile_.has_value()) layoutNeedFrames_.emplace_back(typeOrdinal, LayoutNeeds{});
}

void ProjectObjectLayoutModel::endLayoutNeeds(std::size_t typeOrdinal)
{
    if (layoutNeedFrames_.empty() || layoutNeedFrames_.back().first != typeOrdinal) return;
    if (typePartsInFile(nameLookup_->namedTypes()[typeOrdinal], *snapshotCaptureFile_))
        capturedLayoutNeeds_.insert_or_assign(typeOrdinal, std::move(layoutNeedFrames_.back().second));
    layoutNeedFrames_.pop_back();
}

// EN: Section format: u64 struct count and struct entries, then u64 class count and class entries.
//     A struct entry is its identity key, size, alignment, union/pack/minimum-alignment/explicit
//     offsets, fields and layout needs; a class entry is its identity key, size, alignment, header
//     and base-subobject sizes, reference size and alignment, fields and layout needs. A field is its
//     member, offset, size, alignment, type key and optional bit offset and width; a layout need is a
//     kind byte (1 for a class) and a type key. Entries whose references do not all resolve are
//     skipped and computed.
// ES: Formato de la sección: u64 con el número de structs y sus entradas, luego u64 con el número de
//     clases y sus entradas. Una entrada de struct es su llave de identidad, tamaño, alineación,
//     union/pack/alineación mínima/offsets explícitos, campos y layouts pedidos; una de clase es su
//     llave de identidad, tamaño, alineación, tamaños de cabecera y de subobjeto base, tamaño y
//     alineación de referencia, campos y layouts pedidos. Un campo es su miembro, offset, tamaño,
//     alineación, llave de tipo y offset y ancho de bits opcionales; un layout pedido es un byte de
//     tipo (1 para una clase) y una llave de tipo. Las entradas cuyas referencias no resuelven todas se
//     omiten y se calculan.
void ProjectObjectLayoutModel::readSnapshotLayouts()
{
    for (const PrecompiledSnapshotUnit& snapshot : precompiledSnapshots_)
    {
        const std::string* const payload = precompiledSnapshotSection(snapshot, kLayoutSection);
        if (payload == nullptr) continue;
        PrecompiledSnapshotReader reader(*payload, snapshot.index);
        const std::uint64_t structCount = reader.u64();
        for (std::uint64_t entry = 0; entry < structCount && !reader.failed(); ++entry)
            readSnapshotStructLayout(reader);
        const std::uint64_t classCount = reader.u64();
        for (std::uint64_t entry = 0; entry < classCount && !reader.failed(); ++entry)
            readSnapshotClassLayout(reader);
    }
}

bool ProjectObjectLayoutModel::readSnapshotFields(
    PrecompiledSnapshotReader& reader,
    std::vector<ObjectFieldLayout>& fields)
{
    bool resolved = true;
    const std::uint32_t count = reader.u32();
    for (std::uint32_t index = 0; index < count && !reader.failed(); ++index)
    {
        ObjectFieldLayout field;
        const std::optional<std::size_t> member = readMemberReference(reader, *nameLookup_, *memberModel_);
        field.offset = reader.u64();
        field.size = reader.u64();
        field.alignment = reader.u64();
        field.typeKey = reader.text();
        field.bitOffset = readSnapshotOptional(reader);
        field.bitWidth = readSnapshotOptional(reader);
        resolved = resolved && member.has_value();
        field.memberOrdinal = member.value_or(0);
        fields.push_back(std::move(field));
    }
    return resolved && !reader.failed();
}

bool ProjectObjectLayoutModel::readSnapshotNeeds(PrecompiledSnapshotReader& reader, LayoutNeeds& needs)
{
    bool resolved = true;
    const std::uint32_t count = reader.u32();
    for (std::uint32_t index = 0; index < count && !reader.failed(); ++index)
    {
        const bool isClass = reader.byte() != 0U;
        const std::optional<std::size_t> typeOrdinal = readTypeReference(reader, *nameLookup_);
        resolved = resolved && typeOrdinal.has_value();
        needs.emplace_back(isClass, typeOrdinal.value_or(0));
    }
    return resolved && !reader.failed();
}

bool ProjectObjectLayoutModel::readSnapshotStructLayout(PrecompiledSnapshotReader& reader)
{
    const std::optional<std::size_t> typeOrdinal = readTypeReference(reader, *nameLookup_);
    StructLayoutSnapshotEntry entry;
    StructValueLayout& layout = entry.layout;
    layout.size = reader.u64();
    layout.alignment = reader.u64();
    readSnapshotFlags(reader, {&layout.isUnion, &layout.hasExplicitFieldOffsets});
    layout.pack = reader.u64();
    layout.minimumAlignment = reader.u64();
    bool resolved = readSnapshotFields(reader, layout.fields);
    resolved = readSnapshotNeeds(reader, entry.layoutNeeds) && resolved;
    if (!resolved || !typeOrdinal.has_value() || reader.failed() ||
        snapshotOwningType(nameLookup_->namedTypes()[*typeOrdinal], precompiledSnapshots_) == nullptr)
        return false;
    layout.typeOrdinal = *typeOrdinal;
    layout.typeKey = "type:" + nameLookup_->namedTypes()[*typeOrdinal].identity.key();
    snapshotStructLayouts_.insert_or_assign(*typeOrdinal, std::move(entry));
    return true;
}

bool ProjectObjectLayoutModel::readSnapshotClassLayout(PrecompiledSnapshotReader& reader)
{
    const std::optional<std::size_t> typeOrdinal = readTypeReference(reader, *nameLookup_);
    ClassLayoutSnapshotEntry entry;
    ClassObjectLayout& layout = entry.layout;
    layout.size = reader.u64();
    layout.alignment = reader.u64();
    layout.headerSize = reader.u64();
    layout.baseSubobjectSize = reader.u64();
    layout.referenceSize = reader.u64();
    layout.referenceAlignment = reader.u64();
    bool resolved = readSnapshotFields(reader, layout.fields);
    resolved = readSnapshotNeeds(reader, entry.layoutNeeds) && resolved;
    if (!resolved || !typeOrdinal.has_value() || reader.failed() ||
        snapshotOwningType(nameLookup_->namedTypes()[*typeOrdinal], precompiledSnapshots_) == nullptr)
        return false;
    layout.typeOrdinal = *typeOrdinal;
    layout.typeKey = "type:" + nameLookup_->namedTypes()[*typeOrdinal].identity.key();
    layout.baseTypeOrdinal = inheritanceModel_->baseTypeOrdinal(*typeOrdinal);
    snapshotClassLayouts_.insert_or_assign(*typeOrdinal, std::move(entry));
    return true;
}

// EN: Asks, in order, for every layout the producer's computation of `typeOrdinal` asked for:
//     already built layouts return at once and the others are built (or installed) now, exactly as
//     the computation would have done. False when one of them fails.
// ES: Pide, en orden, cada layout que pidió el cálculo del productor para `typeOrdinal`: los ya
//     construidos regresan de inmediato y los demás se construyen (o instalan) ahora, igual que lo
//     habría hecho el cálculo. False si alguno falla.
bool ProjectObjectLayoutModel::replaySnapshotNeeds(
    std::size_t typeOrdinal,
    const LayoutNeeds& needs,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const SourceRange range = nameLookup_->namedTypes()[typeOrdinal].range;
    bool built = true;
    for (const std::pair<bool, std::size_t>& need : needs)
    {
        if (need.first) built = buildClassLayout(need.second, diagnostics) && built;
        else built = layoutNamedType(need.second, diagnostics, range).has_value() && built;
    }
    return built;
}

bool ProjectObjectLayoutModel::installSnapshotStructLayout(
    std::size_t typeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool& built)
{
    const auto found = snapshotStructLayouts_.find(typeOrdinal);
    if (found == snapshotStructLayouts_.end()) return false;
    StructLayoutSnapshotEntry entry = std::move(found->second);
    snapshotStructLayouts_.erase(found);
    namedLayoutStates_[typeOrdinal] = NamedLayoutState::Visiting;
    built = replaySnapshotNeeds(typeOrdinal, entry.layoutNeeds, diagnostics);
    if (!built)
    {
        namedLayoutStates_[typeOrdinal] = NamedLayoutState::Failed;
        return true;
    }
    namedTypeLayouts_[typeOrdinal] = target::TypeLayout{entry.layout.size, entry.layout.alignment};
    structLayoutIndexByType_[typeOrdinal] = structLayouts_.size();
    structLayouts_.push_back(std::move(entry.layout));
    namedLayoutStates_[typeOrdinal] = NamedLayoutState::Complete;
    ++installedSnapshotStructs_;
    return true;
}

bool ProjectObjectLayoutModel::installSnapshotClassLayout(
    std::size_t typeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool& built)
{
    const auto found = snapshotClassLayouts_.find(typeOrdinal);
    if (found == snapshotClassLayouts_.end()) return false;
    ClassLayoutSnapshotEntry entry = std::move(found->second);
    snapshotClassLayouts_.erase(found);
    built = replaySnapshotNeeds(typeOrdinal, entry.layoutNeeds, diagnostics);
    if (!built) return true;
    classLayoutIndexByType_[typeOrdinal] = classLayouts_.size();
    classLayouts_.push_back(std::move(entry.layout));
    ++installedSnapshotClasses_;
    return true;
}

void ProjectObjectLayoutModel::writeSnapshotFields(
    PrecompiledSnapshotWriter& writer,
    const std::vector<ObjectFieldLayout>& fields,
    bool& written) const
{
    writer.u32(static_cast<std::uint32_t>(fields.size()));
    for (const ObjectFieldLayout& field : fields)
    {
        writeMemberReference(writer, *nameLookup_, *memberModel_, field.memberOrdinal, written);
        writer.u64(field.offset);
        writer.u64(field.size);
        writer.u64(field.alignment);
        writer.text(field.typeKey);
        writeSnapshotOptional(writer, field.bitOffset);
        writeSnapshotOptional(writer, field.bitWidth);
    }
}

void ProjectObjectLayoutModel::writeSnapshotNeeds(
    PrecompiledSnapshotWriter& writer,
    const LayoutNeeds& needs,
    bool& written) const
{
    writer.u32(static_cast<std::uint32_t>(needs.size()));
    for (const std::pair<bool, std::size_t>& need : needs)
    {
        writer.byte(need.first ? 1U : 0U);
        writeTypeReference(writer, *nameLookup_, need.second, written);
    }
}

bool ProjectObjectLayoutModel::writePrecompiledSnapshotLayouts(std::string& payload, std::string& error) const
{
    if (!snapshotCaptureFile_.has_value() || !built_)
    {
        error = "the API unit layouts were not captured";
        return false;
    }
    PrecompiledSyntaxNodeIndex noSyntax;
    PrecompiledSnapshotWriter structs(noSyntax);
    PrecompiledSnapshotWriter classes(noSyntax);
    bool written = true;
    std::uint64_t structCount = 0;
    std::uint64_t classCount = 0;
    for (const StructValueLayout& layout : structLayouts_)
    {
        const auto needs = capturedLayoutNeeds_.find(layout.typeOrdinal);
        if (needs == capturedLayoutNeeds_.end()) continue;
        writeTypeReference(structs, *nameLookup_, layout.typeOrdinal, written);
        structs.u64(layout.size);
        structs.u64(layout.alignment);
        writeSnapshotFlags(structs, {layout.isUnion, layout.hasExplicitFieldOffsets});
        structs.u64(layout.pack);
        structs.u64(layout.minimumAlignment);
        writeSnapshotFields(structs, layout.fields, written);
        writeSnapshotNeeds(structs, needs->second, written);
        ++structCount;
    }
    for (const ClassObjectLayout& layout : classLayouts_)
    {
        const auto needs = capturedLayoutNeeds_.find(layout.typeOrdinal);
        if (needs == capturedLayoutNeeds_.end()) continue;
        written = written && layout.baseTypeOrdinal == inheritanceModel_->baseTypeOrdinal(layout.typeOrdinal);
        writeTypeReference(classes, *nameLookup_, layout.typeOrdinal, written);
        for (const std::uint64_t value : {layout.size, layout.alignment, layout.headerSize, layout.baseSubobjectSize,
                 layout.referenceSize, layout.referenceAlignment})
            classes.u64(value);
        writeSnapshotFields(classes, layout.fields, written);
        writeSnapshotNeeds(classes, needs->second, written);
        ++classCount;
    }
    if (!written || structs.failed() || classes.failed())
    {
        error = "a layout entry of the API unit cannot be written by identity";
        return false;
    }
    PrecompiledSnapshotWriter structHeader(noSyntax);
    structHeader.u64(structCount);
    PrecompiledSnapshotWriter classHeader(noSyntax);
    classHeader.u64(classCount);
    payload = structHeader.take() + structs.take() + classHeader.take() + classes.take();
    return true;
}
}
