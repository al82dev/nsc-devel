#pragma once

#include "nativesharp/Interop/CppSessionDiagnostic.h"
#include "nativesharp/Interop/CppType.h"

#include <optional>
#include <string>

namespace nativesharp::interop
{
// A declaration snapshot owns its type and location; it does not imply that
// NativeSharp can yet bind or use the alias.
struct CppTypeAliasDeclaration
{
    std::string name;
    std::string qualifiedName;
    CppType underlyingType;
    std::optional<CppSourcePoint> location;
    bool insideAnonymousNamespace = false;
};
}
