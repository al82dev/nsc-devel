#include "nativesharp/Frontend/ProjectNameLookup.h"

#include "../Snapshot/PrecompiledSnapshotReader.h"
#include "../Snapshot/PrecompiledSnapshotWriter.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
constexpr std::string_view kDeclarationsSection = "name-declarations";

// EN: The payload of the `name-declarations` section, or nullptr when the snapshot has none.
// ES: El payload de la sección `name-declarations`, o nullptr si el snapshot no la tiene.
const std::string* declarationsSection(const PrecompiledSnapshotUnit& snapshot) noexcept
{
    for (const compilation::PrecompiledSnapshotSection& section : snapshot.snapshot.sections)
    {
        if (section.model == kDeclarationsSection) return &section.payload;
    }
    return nullptr;
}

// EN: True when a declaration recorded by the reference carries exactly what the consumer derives
//     from its own unit and syntax node: ordinals, FileId, precompiled flag and the node's range.
// ES: True si una declaración registrada por la referencia lleva exactamente lo que el consumidor
//     deriva de su propia unidad y nodo: ordinales, FileId, marca de precompilado y el rango del nodo.
template <typename Declaration>
bool derivableFromUnit(const Declaration& declaration, const ProjectSyntaxUnit& unit)
{
    return declaration.syntax != nullptr && declaration.projectOrdinal == unit.projectOrdinal() &&
        declaration.sourceOrdinal == unit.sourceOrdinal() && declaration.fileId == unit.fileId() &&
        declaration.isPrecompiled == unit.isPrecompiled() &&
        declaration.range.begin() == declaration.syntax->range().begin() &&
        declaration.range.end() == declaration.syntax->range().end();
}

// EN: Fills the fields every declaration takes from the consumer unit, as the predeclare functions do.
// ES: Rellena los campos que toda declaración toma de la unidad consumidora, igual que las funciones
//     de predeclaración.
template <typename Declaration>
void deriveFromUnit(Declaration& declaration, const ProjectSyntaxUnit& unit, const std::string& sourcePath)
{
    declaration.projectOrdinal = unit.projectOrdinal();
    declaration.sourceOrdinal = unit.sourceOrdinal();
    declaration.fileId = unit.fileId();
    declaration.range = declaration.syntax->range();
    declaration.sourcePath = sourcePath;
    declaration.isPrecompiled = unit.isPrecompiled();
}

void writeSegments(PrecompiledSnapshotWriter& writer, const std::vector<std::string>& segments)
{
    writer.u32(static_cast<std::uint32_t>(segments.size()));
    for (const std::string& segment : segments) writer.text(segment);
}

std::vector<std::string> readSegments(PrecompiledSnapshotReader& reader)
{
    std::vector<std::string> segments;
    const std::uint32_t count = reader.u32();
    for (std::uint32_t index = 0; index < count && !reader.failed(); ++index) segments.push_back(reader.text());
    return segments;
}

// EN: Writes one predeclared type part: its identity components, kind, accessibility and flags.
// ES: Escribe una parte de tipo predeclarada: componentes de identidad, tipo, accesibilidad y marcas.
void writeTypePart(
    PrecompiledSnapshotWriter& writer,
    const compilation::TypeIdentity& identity,
    const NamedTypeDeclarationPart& part)
{
    writer.node(part.syntax);
    writeSegments(writer, identity.namespaceSegments());
    writer.u32(static_cast<std::uint32_t>(identity.containingTypes().size()));
    for (const compilation::TypeNameSegment& segment : identity.containingTypes())
    {
        writer.text(segment.name);
        writer.u32(segment.arity);
    }
    writer.text(identity.name());
    writer.u32(identity.arity());
    writer.byte(static_cast<std::uint8_t>(part.kind));
    writer.byte(static_cast<std::uint8_t>(part.accessibility));
    for (const bool flag : {part.hasExplicitAccessibility, part.isNested, part.isPartial, part.isAbstract,
             part.isSealed, part.isStatic})
        writer.byte(flag ? 1U : 0U);
}

// EN: Reads one type part written by writeTypePart; the identity's module is the consumer unit's.
// ES: Lee una parte de tipo escrita por writeTypePart; el módulo de la identidad es el de la unidad.
std::optional<compilation::TypeIdentity> readTypePart(
    PrecompiledSnapshotReader& reader,
    const ProjectSyntaxUnit& unit,
    NamedTypeDeclarationPart& part)
{
    part.syntax = reader.node();
    std::vector<std::string> namespaceSegments = readSegments(reader);
    std::vector<compilation::TypeNameSegment> containingTypes;
    const std::uint32_t containingCount = reader.u32();
    for (std::uint32_t index = 0; index < containingCount && !reader.failed(); ++index)
    {
        std::string name = reader.text();
        containingTypes.push_back(compilation::TypeNameSegment{std::move(name), reader.u32()});
    }
    std::string name = reader.text();
    const std::uint32_t arity = reader.u32();
    part.kind = static_cast<NamedTypeDeclarationKind>(reader.byte());
    part.accessibility = static_cast<NamedTypeAccessibility>(reader.byte());
    for (bool* const flag : {&part.hasExplicitAccessibility, &part.isNested, &part.isPartial, &part.isAbstract,
             &part.isSealed, &part.isStatic})
        *flag = reader.byte() != 0U;
    if (reader.failed() || part.syntax == nullptr) return std::nullopt;
    return compilation::TypeIdentity(
        std::string(unit.moduleName()), std::move(namespaceSegments), std::move(containingTypes), std::move(name),
        arity);
}

// EN: Writes one namespace function or state: accessibility, node, namespace, name and its flag.
// ES: Escribe una función o un estado de namespace: accesibilidad, nodo, namespace, nombre y su marca.
template <typename Declaration>
void writeNamespaceMember(PrecompiledSnapshotWriter& writer, const Declaration& declaration, bool flag)
{
    writer.byte(static_cast<std::uint8_t>(declaration.accessibility));
    writer.node(declaration.syntax);
    writer.text(declaration.namespaceName);
    writer.text(declaration.name);
    writer.byte(flag ? 1U : 0U);
}

// EN: Reads one namespace member written by writeNamespaceMember into `declaration` and `flag`.
// ES: Lee un miembro de namespace escrito por writeNamespaceMember en `declaration` y `flag`.
template <typename Declaration>
bool readNamespaceMember(
    PrecompiledSnapshotReader& reader,
    const ProjectSyntaxUnit& unit,
    const std::string& sourcePath,
    Declaration& declaration,
    bool& flag)
{
    declaration.accessibility = static_cast<NamespaceFunctionAccessibility>(reader.byte());
    declaration.syntax = reader.node();
    declaration.namespaceName = reader.text();
    declaration.name = reader.text();
    flag = reader.byte() != 0U;
    if (reader.failed() || declaration.syntax == nullptr) return false;
    deriveFromUnit(declaration, unit, sourcePath);
    declaration.moduleName = std::string(unit.moduleName());
    return true;
}
}

std::size_t ProjectNameLookup::installedDeclarationUnitCount() const noexcept
{
    return installedDeclarationUnits_;
}

void ProjectNameLookup::setPrecompiledSnapshotCaptureUnit(std::size_t unitOrdinal) noexcept
{
    snapshotCaptureUnit_ = unitOrdinal;
}

// EN: Writes the declaration index kept for the captured unit. Every field the consumer derives
//     from its own unit (ordinals, FileId, range, module, precompiled flag) is checked here instead
//     of written, so an installed index is exactly the one indexUnitDeclarations would build.
// ES: Escribe el índice de declaraciones conservado para la unidad capturada. Cada campo que el
//     consumidor deriva de su propia unidad (ordinales, FileId, rango, módulo, marca de precompilado)
//     se comprueba aquí en lugar de escribirse, para que el índice instalado sea exactamente el que
//     construiría indexUnitDeclarations.
bool ProjectNameLookup::writePrecompiledSnapshotDeclarations(
    const PrecompiledSyntaxNodeIndex& index,
    const ProjectSyntaxUnit& unit,
    std::string& payload,
    std::string& error) const
{
    if (!capturedDeclarations_.has_value())
    {
        error = "the API unit declarations were not captured";
        return false;
    }
    const UnitDeclarationIndex& declarations = *capturedDeclarations_;
    PrecompiledSnapshotWriter writer(index);
    bool derivable = true;
    writer.u32(static_cast<std::uint32_t>(declarations.namespaces.size()));
    for (const std::vector<std::string>& namespacePath : declarations.namespaces) writeSegments(writer, namespacePath);
    writer.u64(declarations.pendingTypes.size());
    for (const PendingTypeDeclaration& pending : declarations.pendingTypes)
    {
        derivable = derivable && derivableFromUnit(pending.part, unit) &&
            pending.identity.moduleName() == unit.moduleName();
        writeTypePart(writer, pending.identity, pending.part);
    }
    writer.u64(declarations.namespaceFunctions.size());
    for (const NamespaceFunctionDeclaration& function : declarations.namespaceFunctions)
    {
        derivable = derivable && derivableFromUnit(function, unit) && function.moduleName == unit.moduleName();
        writeNamespaceMember(writer, function, function.hasBody);
    }
    writer.u64(declarations.namespaceStates.size());
    for (const NamespaceStateDeclaration& state : declarations.namespaceStates)
    {
        derivable = derivable && derivableFromUnit(state, unit) && state.moduleName == unit.moduleName();
        writeNamespaceMember(writer, state, state.isConst);
    }
    if (!derivable || writer.failed())
    {
        error = "a declaration of the API unit cannot be derived from its unit";
        return false;
    }
    payload = writer.take();
    return true;
}

// EN: Installs the declaration index the producer computed for this unit, in the producer's order.
//     `index` is only written when the whole section reads.
// ES: Instala el índice de declaraciones que el productor calculó para esta unidad, en el orden del
//     productor. `index` solo se escribe si la sección completa se lee.
bool ProjectNameLookup::installUnitDeclarations(
    const ProjectSyntaxUnit& unit,
    const std::string& sourcePath,
    const PrecompiledSnapshotUnit& snapshot,
    UnitDeclarationIndex& index) const
{
    const std::string* const payload = declarationsSection(snapshot);
    if (payload == nullptr) return false;
    PrecompiledSnapshotReader reader(*payload, snapshot.index);
    UnitDeclarationIndex installed;
    const std::uint32_t namespaceCount = reader.u32();
    for (std::uint32_t entry = 0; entry < namespaceCount && !reader.failed(); ++entry)
        installed.namespaces.push_back(readSegments(reader));
    const std::uint64_t typeCount = reader.u64();
    for (std::uint64_t entry = 0; entry < typeCount && !reader.failed(); ++entry)
    {
        NamedTypeDeclarationPart part;
        std::optional<compilation::TypeIdentity> identity = readTypePart(reader, unit, part);
        if (!identity.has_value()) return false;
        deriveFromUnit(part, unit, sourcePath);
        installed.pendingTypes.push_back(PendingTypeDeclaration{std::move(*identity), std::move(part)});
    }
    const std::uint64_t functionCount = reader.u64();
    for (std::uint64_t entry = 0; entry < functionCount && !reader.failed(); ++entry)
    {
        NamespaceFunctionDeclaration function;
        if (!readNamespaceMember(reader, unit, sourcePath, function, function.hasBody)) return false;
        installed.namespaceFunctions.push_back(std::move(function));
    }
    const std::uint64_t stateCount = reader.u64();
    for (std::uint64_t entry = 0; entry < stateCount && !reader.failed(); ++entry)
    {
        NamespaceStateDeclaration state;
        if (!readNamespaceMember(reader, unit, sourcePath, state, state.isConst)) return false;
        installed.namespaceStates.push_back(std::move(state));
    }
    if (reader.failed() || !reader.atEnd()) return false;
    index = std::move(installed);
    return true;
}
}
