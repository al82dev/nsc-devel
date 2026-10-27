#pragma once

#include "nativesharp/Compilation/PrecompiledSurfaceDemandCollection.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Compilation/ProjectLoadOptions.h"

#include <functional>
#include <string>

namespace nativesharp::frontend
{
class ProjectCompilation;
}

namespace nativesharp::compilation
{
// Applies the existing metadata-backed dependency closure to an already loaded project graph.
// The caller owns project loading and timing; this operation performs no code generation,
// linking, installation, package restore, or metadata writes.
bool preparePrecompiledProject(
    ProjectGraph graph,
    const ProjectLoadOptions& loadOptions,
    frontend::ProjectCompilation& compilation,
    std::string& error);

// EN: Hook called once the metadata-backed dependency closure has been applied to the graph and
//     before the precompiled modules are materialized. It receives the closed graph (read only)
//     and may set `skipMaterialization` when the caller will not compile at all -- for example
//     because a verified compilation bundle will be restored. Returning false aborts preparation
//     with `error`. The hook must not retain the graph reference after it returns.
// ES: Gancho llamado una vez aplicada al grafo la clausura de dependencias respaldada por metadata
//     y antes de materializar los modulos precompilados. Recibe el grafo cerrado (solo lectura) y
//     puede fijar `skipMaterialization` cuando el llamador no compilara -- por ejemplo porque se
//     restaurara un bundle de compilacion verificado. Devolver false aborta la preparacion con
//     `error`. El gancho no debe conservar la referencia al grafo despues de regresar.
using PrecompiledMaterializationGate = std::function<bool(
    const ProjectGraph& closedGraph,
    bool& skipMaterialization,
    std::string& error)>;

// EN: Same as the overload above, but consults `gate` between the closure and the
//     materialization. When the gate sets `skipMaterialization`, no precompiled module is
//     materialized or installed; the closed graph is still installed so `projectGraph()` stays
//     valid. Such a compilation must not be compiled afterwards. An empty gate behaves exactly as
//     the four-parameter overload.
// ES: Igual que la sobrecarga anterior, pero consulta `gate` entre la clausura y la
//     materializacion. Si el gancho fija `skipMaterialization`, no se materializa ni instala
//     ningun modulo precompilado; el grafo cerrado se instala igual para que `projectGraph()` siga
//     siendo valido. Esa compilacion no debe compilarse despues. Un gancho vacio se comporta
//     exactamente como la sobrecarga de cuatro parametros.
bool preparePrecompiledProject(
    ProjectGraph graph,
    const ProjectLoadOptions& loadOptions,
    frontend::ProjectCompilation& compilation,
    std::string& error,
    const PrecompiledMaterializationGate& gate);

// Same as the overload above. When `surfaceDemandSources` is not null, the names written by the
// sources of the closed graph (read through it, or from disk when it has no text) select the
// precompiled declarations that are materialized: each module outside the standard library
// materializes only the declarations those names reach (see PrecompiledSurfaceReachability). A
// source that may need compile-time expansion, or one that cannot be read or tokenized, keeps every
// surface complete. Only code generation passes sources: analysis dumps, checks and the language
// service keep the complete surfaces.
bool preparePrecompiledProject(
    ProjectGraph graph,
    const ProjectLoadOptions& loadOptions,
    frontend::ProjectCompilation& compilation,
    std::string& error,
    const PrecompiledMaterializationGate& gate,
    const PrecompiledSurfaceSourceProvider* surfaceDemandSources);
}
