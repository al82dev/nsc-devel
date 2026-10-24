#pragma once

#include "nativesharp/Compilation/PrecompiledSemanticSnapshot.h"
#include "nativesharp/Compilation/PrecompiledSnapshotDependency.h"
#include "nativesharp/Frontend/PrecompiledSyntaxNodeIndex.h"

#include <span>
#include <string>

namespace nativesharp::frontend
{
// EN: Load-time check of a decoded snapshot header (plan item 1.5). The sidecar codec already
//     verified schema, compiler build id and the library's own NSMETA. This adds: every dependency
//     the producer resolved against is loaded by the consumer with the same NSMETA digest, and the
//     restored API tree has the producer's node count, type count and kind digest. On failure
//     `reason` names the first mismatch and the library takes today's path (rule 0.4). Identities
//     inside the sections are checked by each model before it installs anything.
// ES: Comprobación al cargar de la cabecera de un snapshot decodificado (punto 1.5 del plan). El
//     codec del sidecar ya verificó esquema, build id del compilador y la NSMETA propia de la
//     biblioteca. Esto agrega: cada dependencia contra la que resolvió el productor está cargada en
//     el consumidor con el mismo digest de NSMETA, y el árbol de API restaurado tiene la cantidad de
//     nodos, de tipos y el digest de clases del productor. Si falla, `reason` nombra la primera
//     diferencia y la biblioteca va por la ruta de hoy (regla 0.4). Las identidades dentro de las
//     secciones las comprueba cada modelo antes de instalar nada.
bool validatePrecompiledSnapshot(
    const compilation::PrecompiledSemanticSnapshot& snapshot,
    std::span<const compilation::PrecompiledSnapshotDependency> loadedDependencies,
    const PrecompiledSyntaxNodeIndex& apiIndex,
    std::string& reason);
}
