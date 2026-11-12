#pragma once

#include "nativesharp/Interop/CppSessionDiagnostic.h"
#include "nativesharp/Interop/CppType.h"

#include <optional>
#include <memory>
#include <string>

namespace nativesharp::interop
{
namespace abi_plan { struct Plan; }
// A declaration snapshot owns its type and location; it does not imply that
// NativeSharp can yet bind or use the alias.
struct CppTypeAliasDeclaration
{
    std::string name;
    std::string qualifiedName;
    CppType underlyingType;
    std::optional<CppSourcePoint> location;
    bool insideAnonymousNamespace = false;
    std::optional<std::string> clangUsr;
    std::shared_ptr<const abi_plan::Plan> abiPlan;
};
}
