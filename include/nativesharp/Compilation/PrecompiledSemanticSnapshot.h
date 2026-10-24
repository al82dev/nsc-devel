#pragma once

#include "nativesharp/Compilation/PrecompiledSnapshotDependency.h"
#include "nativesharp/Compilation/PrecompiledSnapshotSection.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::compilation
{
// EN: Semantic snapshot of the public API unit of one library, carried by the `.nsproducts`
//     sidecar (schema v2). The header pins what the entries were resolved against: the NSMETA of
//     every dependency the producer used, and the shape of the API syntax tree (node and type
//     counts plus a digest of their kinds in index order). The sections hold the entries of each
//     semantic model.
// ES: Snapshot semántico de la unidad de API pública de una biblioteca, transportado por el sidecar
//     `.nsproducts` (esquema v2). La cabecera fija contra qué se resolvieron las entradas: la NSMETA
//     de cada dependencia que usó el productor y la forma del árbol sintáctico de la API (cantidad
//     de nodos y de tipos, más un digest de sus clases en orden de índice). Las secciones guardan
//     las entradas de cada modelo semántico.
struct PrecompiledSemanticSnapshot final
{
    std::vector<PrecompiledSnapshotDependency> dependencies;
    std::uint64_t nodeCount = 0;
    std::uint64_t typeCount = 0;
    std::string nodeKindDigest;
    std::vector<PrecompiledSnapshotSection> sections;
};
}
