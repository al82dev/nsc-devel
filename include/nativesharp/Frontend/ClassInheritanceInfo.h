#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
struct ClassInheritanceInfo final
{
    std::size_t typeOrdinal = 0;
    std::optional<std::size_t> baseTypeOrdinal;
    std::string baseTypeKey;
    std::size_t depth = 0;
    std::vector<std::size_t> directlyDerivedTypeOrdinals;
};
}
