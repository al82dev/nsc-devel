#pragma once

#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Frontend/NamedTypeDeclaration.h"
#include "nativesharp/Frontend/PrecompiledSnapshotUnit.h"

#include <span>
#include <string>
#include <string_view>

namespace nativesharp::frontend
{
// EN: The payload of section `model` of an accepted snapshot, or nullptr when it has none.
// ES: El payload de la sección `model` de un snapshot aceptado, o nullptr si no la tiene.
const std::string* precompiledSnapshotSection(
    const PrecompiledSnapshotUnit& snapshot,
    std::string_view model) noexcept;

// EN: Plan item 3.5, producer side: true when the type has parts and every part is in `fileId`.
// ES: Punto 3.5 del plan, lado productor: true si el tipo tiene partes y todas están en `fileId`.
bool typePartsInFile(const NamedTypeDeclaration& type, FileId fileId) noexcept;

// EN: Plan item 3.5, consumer side: the accepted snapshot whose unit holds every part of the type,
//     or nullptr (a type with a part outside a snapshot unit, such as a template part, is computed).
// ES: Punto 3.5 del plan, lado consumidor: el snapshot aceptado cuya unidad tiene todas las partes
//     del tipo, o nullptr (un tipo con una parte fuera de una unidad con snapshot, como una parte de
//     plantillas, se calcula).
const PrecompiledSnapshotUnit* snapshotOwningType(
    const NamedTypeDeclaration& type,
    std::span<const PrecompiledSnapshotUnit> snapshots) noexcept;
}
