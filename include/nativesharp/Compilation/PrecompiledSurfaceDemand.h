#pragma once

#include <string>
#include <unordered_set>

namespace nativesharp::compilation
{
// The names that the source projects of one compilation write. A precompiled declaration that is
// not reachable from these names cannot be used by that source, so its surface does not have to be
// materialized. `enabled` is false when the names do not bound what the source can use (a source
// that needs compile-time expansion, or one that could not be read or tokenized); every precompiled
// surface is then materialized complete.
struct PrecompiledSurfaceDemand final
{
    bool enabled = false;
    std::unordered_set<std::string> names;
};
} // namespace nativesharp::compilation
