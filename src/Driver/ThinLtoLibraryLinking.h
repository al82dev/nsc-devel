#pragma once

#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/LinkTimeOptimizationMode.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/ThinLtoLibraryUnits.h"

#include <cstddef>
#include <filesystem>
#include <ostream>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
class ProjectCompilation;
}

namespace nativesharp::llvm_backend
{
class ProjectOopObjectBundle;
}

namespace nativesharp::driver
{
/// EN: ThinLTO across static libraries (optimization part 2, item 48;
///     NSC-DISENO-SIDECAR-THINLTO-BIBLIOTECAS.md). For a consumer, collect() accepts the sidecars of
///     the project's static library references that match this build byte for byte, the object
///     generator imports from their units, and replaceLinkLibraries() links an archive of their
///     objects in place of each library. Any rejected sidecar keeps the native library and is
///     reported; it is never an error. writeSidecar() is the producer side. One instance serves one
///     generation and is not shared between threads.
/// ES: ThinLTO entre bibliotecas estaticas (parte 2, punto 48; NSC-DISENO-SIDECAR-THINLTO-
///     BIBLIOTECAS.md). Para un consumidor, collect() acepta los sidecars de las referencias a
///     bibliotecas estaticas del proyecto que coinciden byte a byte con este build, el generador de
///     objetos importa de sus unidades y replaceLinkLibraries() enlaza un archivo con sus objetos en
///     lugar de cada biblioteca. Un sidecar rechazado conserva la biblioteca nativa y se informa;
///     nunca es un error. writeSidecar() es el lado productor. Una instancia sirve a una generacion
///     y no se comparte entre hilos.
class ThinLtoLibraryLinking final
{
public:
    /// EN: Reads and checks the sidecar of every static library reference when ThinLTO runs for
    ///     `mode` and `level`. Returns false with `error` only when the compatibility identity of
    ///     this build cannot be computed.
    /// ES: Lee y revisa el sidecar de cada referencia a biblioteca estatica cuando ThinLTO corre con
    ///     `mode` y `level`. Devuelve false con `error` solo si no se puede calcular la identidad de
    ///     compatibilidad de este build.
    bool collect(
        const frontend::ProjectCompilation& compilation,
        llvm_backend::LinkTimeOptimizationMode mode,
        llvm_backend::LlvmOptimizationLevel level,
        llvm_backend::DebugInformationKind debugInformation,
        std::string& error);

    /// EN: Hands the accepted units to the object generator; the instance keeps only the report.
    /// ES: Entrega las unidades aceptadas al generador de objetos; la instancia conserva el informe.
    std::vector<llvm_backend::ThinLtoLibraryUnits> takeLibraries();

    /// EN: Replaces every library whose objects `bundle` carries with a content-addressed archive of
    ///     those objects, at the same position of `linkLibraries`. Returns false with `error`.
    /// ES: Reemplaza cada biblioteca cuyos objetos trae `bundle` por un archivo direccionado por
    ///     contenido con esos objetos, en la misma posicion de `linkLibraries`. Devuelve false con
    ///     `error`.
    bool replaceLinkLibraries(
        const llvm_backend::ProjectOopObjectBundle& bundle,
        std::vector<std::filesystem::path>& linkLibraries,
        std::string& error);

    /// EN: Prints `thinlto-libraries:` and one `thinlto-library-fallback:` line per rejected sidecar
    ///     when ThinLTO ran; nothing otherwise.
    /// ES: Imprime `thinlto-libraries:` y una linea `thinlto-library-fallback:` por sidecar
    ///     rechazado cuando ThinLTO corrio; nada en otro caso.
    void printResult(std::ostream& output) const;

    /// EN: Writes the sidecar of a static library whose bundle kept its units: digests of the staged
    ///     .lib and .nsmeta, the compatibility identity and the units, to `sidecarStage`.
    /// ES: Escribe el sidecar de una biblioteca estatica cuyo paquete conservo sus unidades: digests
    ///     de la .lib y la .nsmeta en staging, la identidad de compatibilidad y las unidades, en
    ///     `sidecarStage`.
    static bool writeSidecar(
        const llvm_backend::ProjectOopObjectBundle& bundle,
        const std::filesystem::path& libraryStage,
        const std::filesystem::path& metadataStage,
        const std::filesystem::path& sidecarStage,
        std::string& error);

private:
    bool active_ = false;
    std::vector<llvm_backend::ThinLtoLibraryUnits> libraries_;
    std::vector<std::string> fallbacks_;
    std::size_t consumed_ = 0;
};
} // namespace nativesharp::driver
