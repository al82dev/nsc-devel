#pragma once

#include <cstdint>
#include <string>

namespace nativesharp::ast
{
class TypeSyntax;
}

namespace nativesharp::frontend
{
struct TypeWork final
{
    const ast::TypeSyntax* source;
    ast::TypeSyntax* parent;
    std::uint8_t slot;  // 0=return, 1=error, 2=argument
    std::string tupleName;
};
}
