#pragma once

#include "nativesharp/Frontend/PrecompiledSnapshotUnit.h"

#include <string>

namespace nativesharp::frontend
{
class ProjectCompilation;

// EN: Producer side of plan item 3.4. Writes the `namespace-members` section of the captured API
//     unit: the symbol of every namespace function and namespace state computed from the unit.
//     Fails (no section) when the reference has diagnostics or a symbol cannot be rebuilt from its
//     unit.
// ES: Lado productor del punto 3.4 del plan. Escribe la sección `namespace-members` de la unidad de
//     API capturada: el símbolo de cada función y estado de namespace calculado desde la unidad.
//     Falla (sin sección) si la referencia tiene diagnósticos o si un símbolo no se puede
//     reconstruir desde su unidad.
bool captureNamespaceMemberSnapshot(
    const ProjectCompilation& reference,
    const PrecompiledSnapshotUnit& capture,
    std::string& payload,
    std::string& error);
}
