#pragma once

#include <string>
#include <vector>

namespace nativesharp::llvm_backend
{
/// EN: Contents of the `<Library>.nsthinlto` sidecar of a static library (optimization part 2, items
///     47 and 48; NSC-DISENO-SIDECAR-THINLTO-BIBLIOTECAS.md). `compatibility` is the identity that a
///     consumer must reproduce byte for byte; `libraryDigest` and `metadataDigest` are the SHA-256
///     (hexadecimal) of the published .lib and .nsmeta; `units` holds the ThinLTO pre-link bitcode of
///     every code unit, with its summary, in the producer's partition order. Since format v2
///     (item 53) `dependencies` lists, one per line, the metadata file name and SHA-256 of every
///     library the producer referenced.
/// ES: Contenido del sidecar `<Biblioteca>.nsthinlto` de una biblioteca estatica (parte 2, puntos 47
///     y 48; NSC-DISENO-SIDECAR-THINLTO-BIBLIOTECAS.md). `compatibility` es la identidad que el
///     consumidor debe reproducir byte a byte; `libraryDigest` y `metadataDigest` son el SHA-256
///     (hexadecimal) de la .lib y la .nsmeta publicadas; `units` guarda el bitcode de pre-enlace
///     ThinLTO de cada unidad de codigo, con su resumen, en el orden de particion del productor.
///     Desde el formato v2 (punto 53) `dependencies` lista, una por linea, el nombre del archivo de
///     metadata y el SHA-256 de cada biblioteca que referencio el productor.
struct ThinLtoLibrarySidecar final
{
    std::string compatibility;
    std::string libraryDigest;
    std::string metadataDigest;
    std::string dependencies;
    std::vector<std::string> units;
};
}
