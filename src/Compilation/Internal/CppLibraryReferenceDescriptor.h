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
    interop::CppLanguageMode language = interop::CppLanguageMode::Cpp;
    std::string languageStandard;
    std::vector<interop::CppIncludePath> includePaths;
    std::vector<interop::CppMacroOption> macros;
    interop::CppVolatileMode volatileMode = interop::CppVolatileMode::Default;
};
}
