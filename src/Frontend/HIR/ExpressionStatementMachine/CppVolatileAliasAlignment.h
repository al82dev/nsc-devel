#pragma once

#include "nativesharp/Frontend/MethodExpressionTypeBinding.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectOopHirNode.h"

#include <cstddef>
#include <optional>
#include <string_view>

namespace nativesharp::frontend::internal
{
// EN: Natural width of a scalar that C volatile accesses may carry atomically; zero otherwise.
// ES: Ancho natural de un escalar que los accesos volatile de C pueden llevar atomicamente; cero si no.
inline std::size_t cVolatileScalarWidth(std::string_view elementType) noexcept
{
    return (elementType == "builtin:bool" || elementType == "builtin:sbyte" || elementType == "builtin:byte") ? 1 :
        (elementType == "builtin:short" || elementType == "builtin:ushort") ? 2 :
        (elementType == "builtin:int" || elementType == "builtin:uint" || elementType == "builtin:float") ? 4 :
        (elementType == "builtin:long" || elementType == "builtin:ulong" || elementType == "builtin:double") ? 8 : 0;
}

inline std::optional<std::size_t> cppVolatileAliasAlignment(
    const MethodExpressionTypeBinding* pointer, std::string_view elementType) noexcept
{
    if (!pointer || !pointer->pointerPointeeVolatile || !pointer->cppVolatilePointeeContract)
        return std::nullopt;
    const std::size_t width = cVolatileScalarWidth(elementType);
    if (!width || pointer->pointerKnownAlignment < width || pointer->pointerKnownAlignment % width != 0)
        return std::nullopt;
    return width;
}

// EN: Volatile mode of the record that owns a C field holding a pointer to a volatile scalar.
//     Returns no value when the field value is not such a pointer or the record mode is unknown.
// ES: Modo volatile del record que posee un campo C con puntero a escalar volatile.
//     No devuelve valor si el campo no es ese puntero o si el modo del record se desconoce.
inline std::optional<bool> cFieldVolatilePointeeMode(
    const ProjectMemberModel* members, const ProjectOopHirNode& field, std::string_view elementType) noexcept
{
    if (!members || field.kind != ProjectOopHirNodeKind::FieldValue || !field.memberOrdinal ||
        *field.memberOrdinal >= members->members().size() || !cVolatileScalarWidth(elementType))
        return std::nullopt;
    const auto& member = members->members()[*field.memberOrdinal];
    const auto& declared = member.declaredTypeKey;
    if (!member.isCppExternal || !member.declaredPointerPointeeVolatile ||
        declared.size() != elementType.size() + 1 || !declared.starts_with(elementType) || !declared.ends_with('*'))
        return std::nullopt;
    return member.cppMicrosoftVolatileSemantics;
}
}
