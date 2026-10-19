#pragma once

#include "nativesharp/Compilation/DeclaredAccessibility.h"
#include "nativesharp/Compilation/TypeDeclarationKind.h"
#include "nativesharp/Compilation/TypeIdentity.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace nativesharp::compilation
{
struct TypePartDescriptor final
{
    TypeIdentity identity;
    TypeDeclarationKind kind = TypeDeclarationKind::Class;
    DeclaredAccessibility accessibility = DeclaredAccessibility::Unspecified;
    bool isPartial = false;
    bool isAbstract = false;
    bool isSealed = false;
    bool isStatic = false;
    std::optional<std::string> baseTypeKey;
    std::size_t sourceOrdinal = 0;
    std::uint32_t sourceOffset = 0;
    std::string sourcePath;
};
}
