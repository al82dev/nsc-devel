#pragma once

#include "nativesharp/LLVM/ThinLtoLibrarySidecar.h"

#include <filesystem>
#include <string>

namespace nativesharp::llvm_backend
{
/// EN: Reads and writes the v1 format of `<Library>.nsthinlto` (NSC-DISENO-SIDECAR-THINLTO-
///     BIBLIOTECAS.md, section 4): signature, version, three strings, the units and a SHA-256 of
///     every preceding byte. Every function is stateless and thread safe. Since item 53 the format
///     is v2: a fourth string, the dependencies, follows the metadata digest; a v1 sidecar is
///     rejected, so its library links natively.
/// ES: Lee y escribe el formato v1 de `<Biblioteca>.nsthinlto` (NSC-DISENO-SIDECAR-THINLTO-
///     BIBLIOTECAS.md, seccion 4): firma, version, tres cadenas, las unidades y un SHA-256 de todos
///     los bytes anteriores. Todas las funciones son sin estado y seguras entre hilos. Desde el
///     punto 53 el formato es v2: una cuarta cadena, las dependencias, sigue al digest de metadata;
///     un sidecar v1 se rechaza, asi que su biblioteca se enlaza nativa.
class ThinLtoLibrarySidecarCodec final
{
public:
    /// EN: The sidecar path of `library`: the same path with the extension `.nsthinlto`.
    /// ES: La ruta del sidecar de `library`: la misma ruta con la extension `.nsthinlto`.
    static std::filesystem::path pathFor(const std::filesystem::path& library);

    /// EN: Writes `sidecar` to `path`, replacing it. Returns false with `error` when it cannot.
    /// ES: Escribe `sidecar` en `path` reemplazandolo. Devuelve false con `error` si no puede.
    static bool write(const std::filesystem::path& path, const ThinLtoLibrarySidecar& sidecar, std::string& error);

    /// EN: Reads `path` into `sidecar`. Returns false with `error` for a missing, truncated,
    ///     oversized or corrupt file, another signature or version, or trailing bytes.
    /// ES: Lee `path` en `sidecar`. Devuelve false con `error` si el archivo falta, esta truncado, es
    ///     demasiado grande o esta corrupto, tiene otra firma o version, o le sobran bytes.
    static bool read(const std::filesystem::path& path, ThinLtoLibrarySidecar& sidecar, std::string& error);
};
}
