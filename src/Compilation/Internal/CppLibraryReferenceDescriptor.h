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
    interop::CppLanguageMode language = interop::CppLanguageMode::Cpp;
    std::string languageStandard;
    std::vector<interop::CppIncludePath> includePaths;
    std::vector<interop::CppMacroOption> macros;
    interop::CppVolatileMode volatileMode = interop::CppVolatileMode::Default;
};
}
