#pragma once

#include "nativesharp/Interop/CppSessionDiagnostic.h"
#include "nativesharp/Interop/CppType.h"

#include <optional>
#include <string>

namespace nativesharp::interop
{
struct CppGlobalDeclaration
{
    std::string name;
    std::string qualifiedName;
    CppType type;
    bool externalLinkage = false;
    bool threadLocal = false;
    bool dllImport = false;
    // A name supplied by Clang does not prove that a linked library defines it.
    std::optional<std::string> externalSymbol;
    std::optional<CppSourcePoint> location;
};
}
