#pragma once

#include "nativesharp/Interop/CppSessionOptions.h"

#include <filesystem>
#include <string>
#include <vector>

namespace nativesharp::compilation
{
// Separate from NativeSharp's metadata-backed LibraryReference contract.
struct CppLibraryReference final
{
    std::filesystem::path headerPath;
    std::filesystem::path libraryPath{};
    std::filesystem::path runtimePath;
    // EN: T08-D04b. With RuntimeTls="ImportDirectory", the DLL as declared; runtimePath is then NSC's adapted copy.
    // ES: T08-D04b. Con RuntimeTls="ImportDirectory", la DLL declarada; runtimePath es entonces la copia adaptada.
    std::filesystem::path originalRuntimePath;
    interop::CppLanguageMode language = interop::CppLanguageMode::Cpp;
    std::string languageStandard;
    std::vector<interop::CppIncludePath> includePaths;
    std::vector<interop::CppMacroOption> macros;
    interop::CppVolatileMode volatileMode = interop::CppVolatileMode::Default;
    // EN: T08-D25. PublicTypes="Header": the library publishes this header's own C types in its NSMeta catalog.
    // ES: T08-D25. PublicTypes="Header": la biblioteca publica en su catalogo NSMeta los tipos C de este encabezado.
    bool publicTypes = false;
    // EN: Item 6.1. Implementation="MACRO": the header is a single-file library (stb, cgltf, dr_mp3) whose
    //     definitions appear with MACRO defined. NSC defines MACRO for the session, compiles that translation unit
    //     once and links its object with the program or library.
    // ES: Item 6.1. Implementation="MACRO": el encabezado es una biblioteca de un archivo (stb, cgltf, dr_mp3) cuyas
    //     definiciones aparecen con MACRO definida. NSC define MACRO para la sesion, compila esa unidad una vez y
    //     enlaza su objeto con el programa o la biblioteca.
    std::string implementationMacro;
};
}
