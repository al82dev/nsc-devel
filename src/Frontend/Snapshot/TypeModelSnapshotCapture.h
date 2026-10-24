#pragma once

#include "nativesharp/Frontend/PrecompiledSnapshotUnit.h"

#include <string>

namespace nativesharp::frontend
{
class ProjectCompilation;

// EN: Producer side of plan items 3.6-3.13: the per-type sections of the captured API unit, written
//     only for types whose parts are all in that unit (plan item 3.5). Each capture fails (no
//     section) when the reference has diagnostics or its model is missing.
// ES: Lado productor de los puntos 3.6-3.13 del plan: las secciones por tipo de la unidad de API
//     capturada, escritas solo para tipos con todas sus partes en esa unidad (punto 3.5). Cada
//     captura falla (sin sección) si la referencia tiene diagnósticos o le falta su modelo.
bool captureInheritanceSnapshot(
    const ProjectCompilation& reference,
    const PrecompiledSnapshotUnit& capture,
    std::string& payload,
    std::string& error);

bool captureInterfaceSnapshot(
    const ProjectCompilation& reference,
    const PrecompiledSnapshotUnit& capture,
    std::string& payload,
    std::string& error);

bool captureDispatchSnapshot(
    const ProjectCompilation& reference,
    const PrecompiledSnapshotUnit& capture,
    std::string& payload,
    std::string& error);
}
