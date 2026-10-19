#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace nativesharp::frontend::internal
{
struct NamedConstructorArgumentMap final
{
    bool valid = false;
    std::vector<std::size_t> explicitParameterOrdinals;
    std::vector<std::size_t> defaultedParameterOrdinals;
    std::vector<bool> parameterPackElements;
    std::optional<std::size_t> parameterPackOrdinal;
    bool usesExpandedParameterPack = false;
    bool synthesizesEmptyParameterPack = false;
};
}
