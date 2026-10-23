#pragma once

#include <string_view>

namespace nativesharp::frontend::internal
{
// Reports an indexed lookup whose result differs from the original linear search, then aborts.
// Only code guarded by `if constexpr (kIndexShadowCheckEnabled)` calls it.
[[noreturn]] void reportIndexShadowMismatch(std::string_view function, std::string_view key);
}
