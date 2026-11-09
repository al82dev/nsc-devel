#pragma once

#include <filesystem>
#include <span>
#include <string>

namespace nativesharp::llvm_backend
{
class LlvmTargetMachine;
}

namespace nativesharp::linker
{
class LldLinker;

/// EN: True when the DLL file name has a character outside ASCII. The Windows loader reads import-table DLL
///     names through the ANSI code page, so such a DLL cannot be imported by name (T08-D01).
/// ES: Verdadero cuando el nombre de archivo de la DLL tiene un caracter fuera de ASCII. El cargador de Windows
///     lee los nombres de DLL de la tabla de importacion con la pagina ANSI, asi que esa DLL no se puede
///     importar por nombre (T08-D01).
bool needsUnicodeDllLoader(const std::filesystem::path& dllPath);

/// EN: Replaces the import library of `dllPath` with a static library that binds every export at its first
///     call by loading the DLL through its UTF-16 path, from the directory of the module that links the
///     library. Each export `X` keeps its import shape: a thunk `X` jumps through the slot `__imp_X`, which
///     starts at a lazy stub; the stub saves the argument registers, resolves the export with LoadLibraryExW and
///     GetProcAddress, stores it in the slot and jumps to it. A missing DLL raises STATUS_DLL_NOT_FOUND and a
///     missing export STATUS_ENTRYPOINT_NOT_FOUND, as the loader would. `directory` holds the temporary object.
/// ES: Sustituye la biblioteca de importacion de `dllPath` por una biblioteca estatica que enlaza cada
///     exportacion en su primera llamada cargando la DLL por su ruta UTF-16, desde el directorio del modulo que
///     enlaza la biblioteca. Cada exportacion `X` conserva la forma de importacion: un thunk `X` salta por la
///     ranura `__imp_X`, que empieza en un stub diferido; el stub guarda los registros de argumentos, resuelve
///     la exportacion con LoadLibraryExW y GetProcAddress, la guarda en la ranura y salta a ella. Una DLL
///     ausente lanza STATUS_DLL_NOT_FOUND y una exportacion ausente STATUS_ENTRYPOINT_NOT_FOUND, como el
///     cargador. `directory` guarda el objeto temporal.
bool writeUnicodeDllLoaderLibrary(
    const LldLinker& linker,
    const llvm_backend::LlvmTargetMachine& targetMachine,
    const std::filesystem::path& directory,
    const std::filesystem::path& dllPath,
    std::span<const std::string> exportSymbols,
    const std::filesystem::path& importLibraryPath,
    std::string& error);
}
