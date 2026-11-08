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
    std::filesystem::path libraryPath;
    std::filesystem::path runtimePath;
    interop::CppLanguageMode language = interop::CppLanguageMode::Cpp;
    std::string languageStandard;
    std::vector<interop::CppIncludePath> includePaths;
    std::vector<interop::CppMacroOption> macros;
    interop::CppVolatileMode volatileMode = interop::CppVolatileMode::Default;
};
}
