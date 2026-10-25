#pragma once

#include "nativesharp/Basic/FileId.h"

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace nativesharp::source
{
/// EN: Records, on the current thread, every source file that SourceManager hands out while the
///     recording is alive (getFile and everything built on it: positions, paths, text). The LLVM
///     object generator wraps the lowering of one code unit in a recording, so the unit's
///     pre-LLVM cache key covers exactly the files whose positions, paths or contents reached its
///     object (optimization part 2, item 51). Recordings nest: a file reaches every active one.
///     Not shared between threads; one SourceManager may serve many threads, each with its own.
/// ES: Registra, en el hilo actual, cada archivo fuente que SourceManager entrega mientras la
///     grabacion vive (getFile y todo lo que se apoya en el: posiciones, rutas, texto). El
///     generador de objetos LLVM envuelve el lowering de una unidad de codigo en una grabacion, asi
///     que la llave pre-LLVM de la unidad cubre exactamente los archivos cuyas posiciones, rutas o
///     contenido llegaron a su objeto (parte 2, punto 51). Las grabaciones se anidan: un archivo
///     llega a todas las activas. No se comparte entre hilos; un SourceManager puede servir a
///     varios hilos, cada uno con la suya.
class SourceAccessRecording final
{
public:
    SourceAccessRecording() noexcept;
    ~SourceAccessRecording();
    SourceAccessRecording(const SourceAccessRecording&) = delete;
    SourceAccessRecording& operator=(const SourceAccessRecording&) = delete;

    /// EN: Called by SourceManager for every file it returns; records it in each active recording.
    /// ES: La llama SourceManager por cada archivo que devuelve; lo registra en cada grabacion activa.
    static void noteAccess(FileId file);

    /// EN: The recorded files, sorted by id value.
    /// ES: Los archivos registrados, ordenados por valor de id.
    std::vector<FileId> files() const;

private:
    SourceAccessRecording* previous_ = nullptr;
    std::unordered_set<std::uint32_t> fileIds_;
};
}
