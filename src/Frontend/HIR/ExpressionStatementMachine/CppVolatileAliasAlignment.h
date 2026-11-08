#pragma once

#include "nativesharp/Frontend/MethodExpressionTypeBinding.h"

#include <cstddef>
#include <optional>
#include <string_view>

namespace nativesharp::frontend::internal
{
inline std::optional<std::size_t> cppVolatileAliasAlignment(
    const MethodExpressionTypeBinding* pointer, std::string_view elementType) noexcept
{
    if (!pointer || !pointer->pointerPointeeVolatile || !pointer->cppVolatilePointeeContract)
        return std::nullopt;
    const std::size_t width =
        (elementType == "builtin:bool" || elementType == "builtin:sbyte" || elementType == "builtin:byte") ? 1 :
        (elementType == "builtin:short" || elementType == "builtin:ushort") ? 2 :
        (elementType == "builtin:int" || elementType == "builtin:uint" || elementType == "builtin:float") ? 4 :
        (elementType == "builtin:long" || elementType == "builtin:ulong" || elementType == "builtin:double") ? 8 : 0;
    if (!width || pointer->pointerKnownAlignment < width || pointer->pointerKnownAlignment % width != 0)
        return std::nullopt;
    return width;
}
}
