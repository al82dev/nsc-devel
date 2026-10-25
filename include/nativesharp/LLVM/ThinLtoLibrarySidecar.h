#pragma once

#include <string>
#include <vector>

namespace nativesharp::llvm_backend
{
/// EN: Contents of the `<Library>.nsthinlto` sidecar of a static library (optimization part 2, items
///     47 and 48; NSC-DISENO-SIDECAR-THINLTO-BIBLIOTECAS.md). `compatibility` is the identity that a
///     consumer must reproduce byte for byte; `libraryDigest` and `metadataDigest` are the SHA-256
///     (hexadecimal) of the published .lib and .nsmeta; `units` holds the ThinLTO pre-link bitcode of
///     every code unit, with its summary, in the producer's partition order.
/// ES: Contenido del sidecar `<Biblioteca>.nsthinlto` de una biblioteca estatica (parte 2, puntos 47
///     y 48; NSC-DISENO-SIDECAR-THINLTO-BIBLIOTECAS.md). `compatibility` es la identidad que el
///     consumidor debe reproducir byte a byte; `libraryDigest` y `metadataDigest` son el SHA-256
///     (hexadecimal) de la .lib y la .nsmeta publicadas; `units` guarda el bitcode de pre-enlace
///     ThinLTO de cada unidad de codigo, con su resumen, en el orden de particion del productor.
struct ThinLtoLibrarySidecar final
{
    std::string compatibility;
    std::string libraryDigest;
    std::string metadataDigest;
    std::vector<std::string> units;
};
}
