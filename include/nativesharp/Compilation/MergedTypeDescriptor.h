#pragma once

#include "nativesharp/Compilation/DeclaredAccessibility.h"
#include "nativesharp/Compilation/TypeDeclarationKind.h"
#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Compilation/TypePartDescriptor.h"

#include <optional>
#include <string>
#include <vector>

namespace nativesharp::compilation
{
struct MergedTypeDescriptor final
{
    TypeIdentity identity;
    TypeDeclarationKind kind = TypeDeclarationKind::Class;
    DeclaredAccessibility accessibility = DeclaredAccessibility::Internal;
    bool isAbstract = false;
    bool isSealed = false;
    bool isStatic = false;
    std::optional<std::string> baseTypeKey;
    std::vector<TypePartDescriptor> parts;
};
}
