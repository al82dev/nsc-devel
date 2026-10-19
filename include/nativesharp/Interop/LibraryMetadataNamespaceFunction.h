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
};
}
