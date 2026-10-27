#pragma once

#include "nativesharp/Basic/FileId.h"

#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nativesharp::source
{
class SourceManager;
}

namespace nativesharp::llvm_backend
{
/// EN: The source half of the pre-LLVM object cache (optimization part 2, item 51). A code unit's
///     object depends on its MIR identity and on the source files its lowering read: positions of
///     debug records and panic sites, paths, checksums. The first is the unit key computed before
///     lowering; the second is only known by recording the lowering (SourceAccessRecording). This
///     index turns a unit key plus the recorded files into the object key, and keeps the list of
///     files of the last lowering of each unit key as a manifest, so the next build can compose the
///     object key before lowering. Thread safety: every member is safe from several threads.
/// ES: La mitad de fuentes del cache de objetos pre-LLVM (parte 2, punto 51). El objeto de una
///     unidad de codigo depende de su identidad MIR y de los archivos fuente que leyo su lowering:
///     posiciones de registros de depuracion y de sitios de panic, rutas, checksums. Lo primero es
///     la llave de unidad calculada antes del lowering; lo segundo solo se conoce grabando el
///     lowering (SourceAccessRecording). Este indice convierte una llave de unidad mas los archivos
///     grabados en la llave del objeto, y guarda la lista de archivos del ultimo lowering de cada
///     llave de unidad como manifiesto, para que el siguiente build arme la llave del objeto antes
///     del lowering. Hilos: todo miembro es seguro desde varios hilos.
class SourceDependencyIndex final
{
public:
    explicit SourceDependencyIndex(const source::SourceManager& sourceManager);

    /// EN: Cache key of the manifest stored for `unitKey`.
    /// ES: Llave de cache del manifiesto guardado para `unitKey`.
    static std::string manifestKey(std::string_view unitKey);

    /// EN: The manifest of `files`: a header and one normalized path per line, sorted.
    /// ES: El manifiesto de `files`: un encabezado y una ruta normalizada por linea, ordenadas.
    std::string manifestText(std::span<const FileId> files) const;

    /// EN: The files of `text` in this compilation; false when the header differs or a listed path
    ///     is not a source of this compilation (the unit then lowers again).
    /// ES: Los archivos de `text` en esta compilacion; false si el encabezado difiere o una ruta
    ///     listada no es fuente de esta compilacion (la unidad se vuelve a bajar entonces).
    bool manifestFiles(std::string_view text, std::vector<FileId>& files) const;

    /// EN: Object key of `unitKey` with `files`: the path, size and SHA-256 of every file, sorted by
    ///     path, so a file keeps its share of the key when other files are added or removed.
    /// ES: Llave de objeto de `unitKey` con `files`: la ruta, tamano y SHA-256 de cada archivo,
    ///     ordenados por ruta, asi que un archivo conserva su parte de la llave aunque se agreguen o
    ///     quiten otros.
    std::string objectKey(std::string_view unitKey, std::span<const FileId> files) const;

private:
    const std::string& digest(FileId file) const;
    void computeDigest(FileId file) const;

    const source::SourceManager& sourceManager_;
    std::vector<std::string> paths_;
    std::unordered_map<std::string, FileId> filesByPath_;
    mutable std::unique_ptr<std::once_flag[]> digestOnce_;
    mutable std::vector<std::string> digests_;
};
}
