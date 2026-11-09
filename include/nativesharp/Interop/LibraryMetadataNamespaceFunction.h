#pragma once

#include "nativesharp/Interop/LibraryMetadataEscapeClass.h"
#include "nativesharp/Interop/LibraryMetadataOwnershipKind.h"
#include "nativesharp/Interop/LibraryMetadataParameter.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::interop
{
struct LibraryMetadataNamespaceFunction final
{
    std::uint8_t accessibility = 0;
    std::uint8_t flags = 0;
    std::string namespaceName;
    std::string name;
    std::string signatureKey;
    std::string declaredTypeKey;
    std::string declaredErrorTypeKey;
    LibraryMetadataOwnershipKind declaredOwnership = LibraryMetadataOwnershipKind::None;
    std::string mangledName;
    std::vector<LibraryMetadataParameter> parameters;
    std::string returnLifetimeName;
    std::vector<std::string> lifetimeParameters;
    std::vector<std::pair<std::string, std::string>> lifetimeOutlivesConstraints;
    std::vector<LibraryMetadataEscapeClass> parameterEscapeSummary;
    // EN: T08-D02. Qualifiers of the pointee of a pointer-typed result, which the type key does not
    //     carry: kMetadataPointeeConst and kMetadataPointeeVolatile. Zero for every other type.
    // ES: T08-D02. Calificadores del dato apuntado de una declaracion de tipo puntero, que la clave de tipo
    //     no lleva: kMetadataPointeeConst y kMetadataPointeeVolatile. Cero para cualquier otro tipo.
    std::uint8_t declaredPointeeQualifiers = 0;
};
}
