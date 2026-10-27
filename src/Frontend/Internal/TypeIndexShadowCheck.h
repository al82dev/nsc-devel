#pragma once

#include "IndexShadowCheckSwitch.h"
#include "IndexShadowMismatch.h"

#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <cstddef>
#include <optional>
#include <string_view>

namespace nativesharp::frontend::internal
{
// Validation builds only (kIndexShadowCheckEnabled): when the shared type index is built, the
// type it returns for identity must be the first named type with that identity, which is what
// the scans it replaced returned. A difference aborts through reportIndexShadowMismatch.
inline void shadowCheckTypeIdentityIndex(
    const ProjectNameLookup& names,
    std::string_view identity,
    std::string_view function)
{
    if (!names.hasTypeIndex()) return;
    const std::optional<std::size_t> ordinal = names.typeOrdinalForIdentity(identity);
    const NamedTypeDeclaration* indexed =
        ordinal.has_value() ? names.typeForOrdinal(*ordinal) : nullptr;
    const NamedTypeDeclaration* scanned = nullptr;
    for (const NamedTypeDeclaration& type : names.namedTypes())
    {
        if (type.identity.key() == identity)
        {
            scanned = &type;
            break;
        }
    }
    if (indexed != scanned) reportIndexShadowMismatch(function, identity);
}
}
