#pragma once

#include "nativesharp/Frontend/PrecompiledSnapshotUnit.h"

#include <string>

namespace nativesharp::frontend
{
class ProjectCompilation;

// EN: Producer side of plan item 3.3. Writes the `members` section of the captured API unit: the
//     symbol of every class, struct and interface member collected from the unit. Fails (no
//     section) when the reference has diagnostics or a member cannot be rebuilt from its unit.
// ES: Lado productor del punto 3.3 del plan. Escribe la sección `members` de la unidad de API
//     capturada: el símbolo de cada miembro de clase, struct e interfaz recolectado de la unidad.
//     Falla (sin sección) si la referencia tiene diagnósticos o si un miembro no se puede
//     reconstruir desde su unidad.
bool captureMemberSnapshot(
    const ProjectCompilation& reference,
    const PrecompiledSnapshotUnit& capture,
    std::string& payload,
    std::string& error);
}
