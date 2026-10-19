#pragma once

#include "nativesharp/Basic/SourceRange.h"

#include <string>
#include <vector>

namespace nativesharp::ast
{
class TypeSyntax;

// NSC-LIMIT-007. Una clausula `where T : class, Base, IFoo` tal como se escribio.
//
// SE GUARDA SIN RESOLVER, y esa es la decision: los tipos quedan como `TypeSyntax*`, no como
// claves de tipo ni como ordinales. Cuando el parser lee la clausula no existe todavia el name
// lookup --se construye tres fases mas tarde-- asi que resolver aqui exigiria una copia privada
// de la resolucion de nombres, que es justo la clase de duplicado que este proyecto persigue.
// Quien valida es una fase semantica posterior, con la herencia y las interfaces ya construidas.
struct GenericConstraintClause
{
    // El parametro que la clausula restringe. Se comprueba contra genericParameters() en el
    // parser: `where U : class` sobre un tipo que no declara `U` es un error de sintaxis, no una
    // condicion que falle mas tarde con un tipo concreto.
    std::string parameterName;
    SourceRange parameterRange;
    // `class` escrito en primera posicion. Es el unico constraint que NO es un tipo, y por eso
    // viaja como bandera y no dentro de `typeConstraints`.
    bool requiresClass = false;
    // NativeSharp 3 declarative capability constraints. They remain orthogonal flags so their
    // canonical identity is independent of source order and spelling duplication.
    bool requiresValueType = false;
    bool requiresDefaultConstructor = false;
    bool requiresCopy = false;
    bool requiresMove = false;
    bool requiresTrackedLifetime = false;
    bool requiresUnmanaged = false;
    bool requiresNotNull = false;
    // NS304-T11-F05: `where T : ref-like` authorizes a stack-only (ref-like) generic argument.
    bool requiresRefLike = false;
    // `where T == U` and `where N == 4`. A const value uses the same deterministic TypeSyntax
    // marker as a const-generic argument; a type/parameter uses ordinary type syntax and is
    // substituted by the generic cloner.
    TypeSyntax* equalityConstraint = nullptr;
    // T05-F03. Left side of `where T.Item == Type`. Null for ordinary parameter equality
    // (`where T == U`). The generic cloner normalizes this projection after substituting T and
    // exposes the resulting concrete type through substitutedArgument.
    TypeSyntax* associatedProjection = nullptr;
    // La clase base y las interfaces, en el orden escrito. Cual es cual no se decide aqui: eso
    // necesita saber que declara cada nombre, y el parser no lo sabe.
    std::vector<TypeSyntax*> typeConstraints;
    // La clausula entera, `where` incluido, para el diagnostico.
    SourceRange range;
    // NSC-LIMIT-007. SOLO EN UNA CLAUSULA CLONADA: el tipo con el que se sustituyo el parametro.
    //
    // Existe porque el argumento de una especializacion --el `Valor` de `Caja<Valor>`-- se
    // BORRA del arbol al materializar (`clearTypeArguments`), asi que la resolucion de nombres,
    // que corre despues, no lo ve nunca y nadie puede preguntar de que tipo es. Colgandolo de la
    // clausula clonada --que SI esta en el arbol-- el argumento vuelve a pasar por la resolucion
    // ordinaria, y la validacion no necesita una segunda copia de la resolucion de nombres.
    const TypeSyntax* substitutedArgument = nullptr;
};
}
