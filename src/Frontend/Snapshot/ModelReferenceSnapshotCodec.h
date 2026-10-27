#pragma once

#include "PrecompiledSnapshotReader.h"
#include "PrecompiledSnapshotWriter.h"

#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace nativesharp::frontend
{
// EN: Plan item 1.4 for the per-type sections. A type is written by its identity key. A member is
//     written as its declaring type's identity key, its signature key and its position in that
//     type's member list; it reads back only when the type resolves, the position exists and the
//     member there has the same signature. A false `written` makes the producer drop the section.
// ES: Punto 1.4 del plan para las secciones por tipo. Un tipo se escribe por su llave de
//     identidad. Un miembro se escribe como la llave de identidad de su tipo, su llave de firma y
//     su posición en la lista de miembros de ese tipo; se lee solo si el tipo resuelve, la posición
//     existe y el miembro ahí tiene la misma firma. Un `written` en false hace que el productor
//     descarte la sección.
void writeTypeReference(
    PrecompiledSnapshotWriter& writer,
    const ProjectNameLookup& names,
    std::size_t typeOrdinal,
    bool& written);
std::optional<std::size_t> readTypeReference(PrecompiledSnapshotReader& reader, const ProjectNameLookup& names);
void writeTypeReferences(
    PrecompiledSnapshotWriter& writer,
    const ProjectNameLookup& names,
    std::span<const std::size_t> typeOrdinals,
    bool& written);
bool readTypeReferences(
    PrecompiledSnapshotReader& reader,
    const ProjectNameLookup& names,
    std::vector<std::size_t>& typeOrdinals);

void writeMemberReference(
    PrecompiledSnapshotWriter& writer,
    const ProjectNameLookup& names,
    const ProjectMemberModel& members,
    std::size_t memberOrdinal,
    bool& written);
std::optional<std::size_t> readMemberReference(
    PrecompiledSnapshotReader& reader,
    const ProjectNameLookup& names,
    const ProjectMemberModel& members);
void writeMemberReferences(
    PrecompiledSnapshotWriter& writer,
    const ProjectNameLookup& names,
    const ProjectMemberModel& members,
    std::span<const std::size_t> memberOrdinals,
    bool& written);
bool readMemberReferences(
    PrecompiledSnapshotReader& reader,
    const ProjectNameLookup& names,
    const ProjectMemberModel& members,
    std::vector<std::size_t>& memberOrdinals);
void writeOptionalMemberReference(
    PrecompiledSnapshotWriter& writer,
    const ProjectNameLookup& names,
    const ProjectMemberModel& members,
    const std::optional<std::size_t>& memberOrdinal,
    bool& written);
bool readOptionalMemberReference(
    PrecompiledSnapshotReader& reader,
    const ProjectNameLookup& names,
    const ProjectMemberModel& members,
    std::optional<std::size_t>& memberOrdinal);
}
