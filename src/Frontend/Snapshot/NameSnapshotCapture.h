#pragma once

#include "nativesharp/Frontend/PrecompiledSnapshotUnit.h"

#include <string>

namespace nativesharp::frontend
{
class ProjectCompilation;

// EN: Producer side of plan item 3.1. Writes the `names` section of the captured API unit: every
//     type-name binding and expression-type binding the reference compilation resolved for the
//     unit, in order, with type ordinals replaced by identity keys. Fails (no section) when the
//     reference has diagnostics, the unit declares type aliases (binding rewrites aliased syntax in
//     place) or the unit's bindings are not one contiguous block.
// ES: Lado productor del punto 3.1 del plan. Escribe la sección `names` de la unidad de API
//     capturada: cada binding de nombre de tipo y de tipo de expresión que la compilación de
//     referencia resolvió para la unidad, en orden, con los ordinales de tipo cambiados por llaves
//     de identidad. Falla (sin sección) si la referencia tiene diagnósticos, si la unidad declara
//     alias de tipo (el binding reescribe en su lugar la sintaxis con alias) o si los bindings de la
//     unidad no forman un solo bloque contiguo.
bool captureNameSnapshot(
    const ProjectCompilation& reference,
    const PrecompiledSnapshotUnit& capture,
    std::string& payload,
    std::string& error);
}
