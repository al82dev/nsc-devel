#include "nativesharp/Frontend/ProjectNameLookup.h"

#include "../Snapshot/PrecompiledSnapshotReader.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
constexpr std::string_view kNamesSection = "names";

// EN: Reads a list of type identity keys and resolves each to this compilation's type ordinal.
// ES: Lee una lista de llaves de identidad de tipo y resuelve cada una al ordinal de tipo de esta
//     compilación.
bool readTypeOrdinals(
    PrecompiledSnapshotReader& reader,
    const ProjectNameLookup& names,
    std::vector<std::size_t>& ordinals)
{
    const std::uint32_t count = reader.u32();
    for (std::uint32_t index = 0; index < count && !reader.failed(); ++index)
    {
        const std::optional<std::size_t> ordinal = names.typeOrdinalForIdentity(reader.typeKey());
        if (!ordinal.has_value()) return false;
        ordinals.push_back(*ordinal);
    }
    return !reader.failed();
}

// EN: The payload of the `names` section, or nullptr when the snapshot has none.
// ES: El payload de la sección `names`, o nullptr si el snapshot no la tiene.
const std::string* namesSection(const PrecompiledSnapshotUnit& snapshot) noexcept
{
    for (const compilation::PrecompiledSnapshotSection& section : snapshot.snapshot.sections)
    {
        if (section.model == kNamesSection) return &section.payload;
    }
    return nullptr;
}

// EN: Reads one type-name binding; the project ordinal and FileId are the consumer unit's own.
// ES: Lee un binding de nombre de tipo; el ordinal de proyecto y el FileId son los de la unidad.
bool readTypeNameBinding(
    PrecompiledSnapshotReader& reader,
    const ProjectNameLookup& names,
    const ProjectSyntaxUnit& unit,
    TypeNameBinding& binding)
{
    binding.syntax = reader.type();
    binding.kind = static_cast<TypeNameBindingKind>(reader.byte());
    binding.projectOrdinal = unit.projectOrdinal();
    binding.fileId = unit.fileId();
    binding.namespaceName = reader.text();
    binding.writtenName = reader.text();
    if (reader.byte() != 0U)
    {
        binding.targetTypeOrdinal = names.typeOrdinalForIdentity(reader.typeKey());
        if (!binding.targetTypeOrdinal.has_value()) return false;
    }
    return binding.syntax != nullptr && readTypeOrdinals(reader, names, binding.candidateTypeOrdinals) &&
        readTypeOrdinals(reader, names, binding.enclosingTypeOrdinals);
}

// EN: Reads one expression-type binding.
// ES: Lee un binding de tipo de expresión.
bool readExpressionBinding(
    PrecompiledSnapshotReader& reader,
    const ProjectNameLookup& names,
    ExpressionTypeNameBinding& binding)
{
    binding.expression = reader.node();
    binding.writtenName = reader.text();
    return binding.expression != nullptr && readTypeOrdinals(reader, names, binding.candidateTypeOrdinals) &&
        readTypeOrdinals(reader, names, binding.enclosingTypeOrdinals);
}
}

void ProjectNameLookup::setPrecompiledSnapshots(std::span<const PrecompiledSnapshotUnit> snapshots) noexcept
{
    precompiledSnapshots_ = snapshots;
}

std::size_t ProjectNameLookup::installedSnapshotUnitCount() const noexcept
{
    return installedSnapshotUnits_;
}

const PrecompiledSnapshotUnit* ProjectNameLookup::snapshotForUnit(std::size_t unitOrdinal) const noexcept
{
    for (const PrecompiledSnapshotUnit& snapshot : precompiledSnapshots_)
    {
        if (snapshot.unitOrdinal == unitOrdinal) return &snapshot;
    }
    return nullptr;
}

// EN: Installs the bindings the producer resolved for this unit, in the producer's order. Nothing
//     is kept unless every entry reads and every identity resolves; `index` is only written on
//     success, and bindingBySyntax keeps the first entry of a syntax exactly as bindUnit does.
// ES: Instala los bindings que el productor resolvió para esta unidad, en el orden del productor.
//     No se conserva nada salvo que cada entrada se lea y cada identidad resuelva; `index` solo se
//     escribe si todo sale bien, y bindingBySyntax conserva la primera entrada de una sintaxis igual
//     que bindUnit.
bool ProjectNameLookup::installUnitBindings(
    const ProjectSyntaxUnit& unit,
    const PrecompiledSnapshotUnit& snapshot,
    UnitBindingIndex& index) const
{
    const std::string* const payload = namesSection(snapshot);
    if (payload == nullptr) return false;
    PrecompiledSnapshotReader reader(*payload, snapshot.index);
    UnitBindingIndex installed;
    const std::uint64_t bindingCount = reader.u64();
    for (std::uint64_t entry = 0; entry < bindingCount && !reader.failed(); ++entry)
    {
        TypeNameBinding binding;
        if (!readTypeNameBinding(reader, *this, unit, binding)) return false;
        installed.bindingBySyntax.emplace(binding.syntax, installed.bindings.size());
        installed.bindings.push_back(std::move(binding));
    }
    const std::uint64_t expressionCount = reader.u64();
    for (std::uint64_t entry = 0; entry < expressionCount && !reader.failed(); ++entry)
    {
        ExpressionTypeNameBinding binding;
        if (!readExpressionBinding(reader, *this, binding)) return false;
        installed.expressionTypeBindingBySyntax.emplace(binding.expression, installed.expressionTypeBindings.size());
        installed.expressionTypeBindings.push_back(std::move(binding));
    }
    if (reader.failed() || !reader.atEnd()) return false;
    index = std::move(installed);
    return true;
}
}
