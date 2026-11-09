#pragma once

#include "nativesharp/Frontend/ProjectMemberModel.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace nativesharp::frontend::method_body_detail
{
// EN: T08-D13. Width of the volatile C scalar reached through a C pointer value whose type key is `T*`, or an
//     array of such pointers (`T*[n]...`); zero when the pointee is not a scalar C volatile accesses can carry.
//     C assumes such a pointer is naturally aligned, so this width is the alias alignment the contract carries.
// ES: T08-D13. Ancho del escalar C volatile al que llega un puntero C de tipo `T*`, o un arreglo de esos punteros;
//     cero si el apuntado no es un escalar que los accesos volatile de C puedan llevar. C supone ese puntero
//     alineado naturalmente, asi que este ancho es la alineacion que lleva el contrato del alias.
inline std::size_t cVolatilePointeeScalarWidth(std::string_view typeKey)
{
    std::string key(typeKey);
    unsigned depth = 0;
    while (const auto element = ProjectMemberModel::fixedArrayElementTypeKey(key))
    {
        if (++depth > 64) return 0;
        key = *element;
    }
    if (!key.ends_with('*')) return 0;
    key.pop_back();
    if (key == "builtin:bool" || key == "builtin:sbyte" || key == "builtin:byte") return 1;
    if (key == "builtin:short" || key == "builtin:ushort") return 2;
    if (key == "builtin:int" || key == "builtin:uint" || key == "builtin:float") return 4;
    if (key == "builtin:long" || key == "builtin:ulong" || key == "builtin:double") return 8;
    return 0;
}
}
