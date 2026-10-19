#pragma once

#include <cstddef>
#include <optional>

namespace nativesharp::frontend
{
struct ProjectInterfaceImplementingType final
{
    std::size_t ordinal = 0;
    std::optional<std::size_t> baseOrdinal;
    std::size_t depth = 0;
    bool valueType = false;
};
}
