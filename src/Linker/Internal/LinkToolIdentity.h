#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace nativesharp::linker
{
/// EN: Appends one line that identifies a tool or toolchain file read by a link step to the key
///     material of a cache that stores link products (static archives, incrementally reused
///     executables): "link-tool <role> <file name>:<size>:<SHA-256>".
///     Parameters: `role`, a stable name such as "linker" or "librarian"; `tool`, the file the
///     link step would use, empty when it was not found; `material`, the key material that
///     receives the line; `error`, the failure text.
///     Returns false only when an existing file cannot be hashed. A missing or empty `tool` adds
///     "link-tool <role> unavailable" and returns true, so the key differs from every stored one
///     and the link step itself reports the real error.
///     Effects: hashes each (path, size, write time) once per process; thread safe.
///     Why: a cache that restores a link product replaces the link step, so its key must change
///     when the linker, the librarian or a toolchain file it reads changes (2026-10-24: a new
///     NSLink was never run for unchanged inputs).
/// ES: Agrega al material de llave de un cache que guarda productos de enlace (archivos estaticos,
///     ejecutables reutilizados de forma incremental) una linea que identifica una herramienta o un
///     archivo del toolchain que lee el enlace: "link-tool <rol> <nombre>:<tamano>:<SHA-256>".
///     Parametros: `role`, un nombre estable como "linker" o "librarian"; `tool`, el archivo que
///     usaria el enlace, vacio si no se encontro; `material`, el material de llave que recibe la
///     linea; `error`, el texto de falla.
///     Retorno: false solo cuando un archivo existente no se puede hashear. Un `tool` ausente o
///     vacio agrega "link-tool <rol> unavailable" y devuelve true, de modo que la llave difiere de
///     toda llave guardada y el propio enlace reporta el error real.
///     Efectos: hashea cada (ruta, tamano, fecha de escritura) una vez por proceso; seguro entre
///     hilos.
///     Motivo: un cache que restaura un producto de enlace sustituye al enlace, asi que su llave
///     debe cambiar cuando cambia el linker, el bibliotecario o un archivo del toolchain que lee
///     (2026-10-24: un NSLink nuevo no se ejecutaba con entradas sin cambios).
bool appendLinkToolIdentity(
    std::string_view role,
    const std::filesystem::path& tool,
    std::string& material,
    std::string& error);
}
