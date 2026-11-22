#pragma once

#include "nativesharp/Interop/CppSessionOptions.h"

#include <optional>
#include <string>
#include <vector>

namespace nativesharp::compilation
{
struct CppLibraryReferenceDescriptor final
{
    std::string header;
    std::optional<std::string> library;
    std::optional<std::string> runtime;
    // EN: T08-D04b. RuntimeTls="ImportDirectory": distribute an adapted copy with an empty import directory.
    // ES: T08-D04b. RuntimeTls="ImportDirectory": distribuir una copia adaptada con directorio de importacion vacio.
    bool runtimeTlsImportDirectory = false;
    // EN: T08-D25. PublicTypes="Header": publish the header's own C types with the library.
    // ES: T08-D25. PublicTypes="Header": publicar con la biblioteca los tipos C propios del encabezado.
    bool publicTypes = false;
    // EN: Item 6.1. Implementation="MACRO": compile the header's definitions (single-file library) with MACRO.
    // ES: Item 6.1. Implementation="MACRO": compilar las definiciones del encabezado (biblioteca de un archivo).
    std::string implementationMacro;
    // Item 18.8: further link libraries of the reference (`<Library Path="SDL2main.lib" />`).
    // Item 18.8: otras bibliotecas de enlace de la referencia.
    std::vector<std::string> additionalLibraries;
    interop::CppLanguageMode language = interop::CppLanguageMode::Cpp;
    std::string languageStandard;
    std::vector<interop::CppIncludePath> includePaths;
    std::vector<interop::CppMacroOption> macros;
    interop::CppVolatileMode volatileMode = interop::CppVolatileMode::Default;
};
}
