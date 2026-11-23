#pragma once

#include "nativesharp/Interop/CppContract.h"

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
    // EN: Rtti="false": the library is compiled without RTTI (MSVC /GR-, Clang -fno-rtti); NSC's Clang session
    //     uses the same option, so the vtables NSC emits need no `type_info` from the C runtime.
    // ES: Rtti="false": la biblioteca se compila sin RTTI; la sesion de Clang de NSC usa la misma opcion.
    bool rtti = true;
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
    // EN: Item 18.8. Further import or static libraries the header needs at link time, in declaration order
    //     (`<Library Path="SDL2main.lib" />` next to `Library="SDL2.lib"`).
    // ES: Item 18.8. Otras bibliotecas de importacion o estaticas que el encabezado necesita al enlazar, en el orden
    //     declarado.
    std::vector<std::filesystem::path> additionalLibraryPaths;
    // EN: R1 (D6). Boundary contracts declared next to the reference (`<Contract>`).
    // ES: R1 (D6). Contratos de frontera declarados junto a la referencia (`<Contract>`).
    std::vector<interop::CppContract> contracts;
};
}
