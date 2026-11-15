#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/AST/TypeModifierSyntax.h"

#include <string>

namespace nativesharp::frontend::internal
{
// EN: LKF-371: the spelling of a fixed-array modifier in a type identity. The extent is part of the
//     type, so Box<int[2]> and Box<int[8]> must not share an identity. A literal extent is written as
//     its value and a named one (a const generic parameter) as its name; an extent of another form
//     keeps the previous `[fixed]` spelling.
// ES: LKF-371: la escritura de un modificador de arreglo fijo en la identidad de un tipo. La extensión
//     forma parte del tipo, así que Box<int[2]> y Box<int[8]> no deben compartir identidad. Una
//     extensión literal se escribe con su valor y una con nombre (un parámetro const genérico) con su
//     nombre; otra forma conserva la escritura anterior `[fixed]`.
inline std::string fixedArrayModifierSpelling(const ast::TypeModifierSyntax& modifier)
{
    const ast::AstNode* count = modifier.arrayCount;
    if (count != nullptr && count->kind() == ast::AstNodeKind::IntegerLiteralExpression)
        return "[" + std::to_string(count->integerValue()) + "]";
    if (count != nullptr && count->kind() == ast::AstNodeKind::IdentifierExpression && !count->text().empty())
        return "[" + std::string(count->text()) + "]";
    return "[fixed]";
}
}
