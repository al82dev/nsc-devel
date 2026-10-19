#pragma once

namespace nativesharp::ast
{
class TypeSyntax;
}

namespace nativesharp::frontend
{
struct ProjectInterfaceDirectSource final
{
    const ast::TypeSyntax* syntax = nullptr;
    bool explicitImpl = false;
};
}
