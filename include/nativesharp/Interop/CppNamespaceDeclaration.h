#pragma once

#include "nativesharp/Interop/CppSessionDiagnostic.h"

#include <optional>
#include <string>

namespace nativesharp::interop
{
struct CppNamespaceDeclaration
{
    std::string qualifiedName;
    bool isInline = false;
    bool isAnonymous = false;
    bool insideAnonymous = false;
    std::optional<CppSourcePoint> location;
    std::optional<std::string> clangUsr;
};
}
