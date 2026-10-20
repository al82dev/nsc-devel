#pragma once

#include "nativesharp/Frontend/MethodScopeKind.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace nativesharp::frontend
{
struct MethodScope final
{
    MethodScopeKind kind = MethodScopeKind::Method;
    std::size_t methodMemberOrdinal = 0;
    std::optional<std::size_t> parentScopeOrdinal;
    std::size_t depth = 0;
    std::vector<std::size_t> localOrdinals;
};
}
