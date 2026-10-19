#pragma once

#include "nativesharp/Basic/SourceRange.h"

#include <string>

namespace nativesharp::frontend
{
struct GenericTemplateExternalTypeReference final
{
    SourceRange range;
    // Canonical named identity, including its owning module. Modifiers are not part of this key;
    // the original template source keeps pointer/array syntax around the rewritten base name.
    std::string typeKey;
};
}
