#pragma once

#include "nativesharp/AST/ReadonlyReferenceMarker.h"
#include "nativesharp/Frontend/ClassMemberSymbol.h"

namespace nativesharp::frontend
{
// EN: Freeze 3.0 P3. True for a callable declared `ref readonly T`: its call yields an addressable
//     but non-assignable place that behaves like an `in` parameter (reads and readonly members
//     only). Derived from the declaration syntax, so a symbol rebuilt from a precompiled snapshot
//     answers the same as one built from source.
// ES: Congelamiento 3.0 P3. Verdadero para un callable declarado `ref readonly T`: su llamada da un
//     lugar direccionable pero no asignable que se comporta como un parametro `in` (solo lecturas y
//     miembros readonly). Se deriva de la sintaxis de la declaracion, asi que un simbolo
//     reconstruido de un snapshot precompilado responde igual que uno construido del fuente.
inline bool isReadonlyReferenceReturn(const ClassMemberSymbol& member)
{
    return member.isReferenceReturn && member.syntax != nullptr &&
        ast::hasReadonlyReferenceMarker(*member.syntax);
}
}
