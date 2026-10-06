#pragma once

#include "nativesharp/Frontend/MethodEscapeOriginKind.h"

#include <cstddef>
#include <optional>
#include <string>

namespace nativesharp::frontend
{
struct MethodEscapeOrigin final
{
    MethodEscapeOriginKind kind = MethodEscapeOriginKind::ExternalStorage;
    std::optional<std::size_t> ordinal;
    // Item 316. The broadest lexical scope in which the referenced storage is guaranteed live.
    // Every alias declaration must be nested in this scope.
    std::optional<std::size_t> maximumValidScopeOrdinal;
    // T06-F04. `this`, `static`, a declared name, or a synthetic `$pN` produced by elision.
    std::string lifetimeName;
};
}
