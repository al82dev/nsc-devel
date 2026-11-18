#pragma once

#include <string>

namespace nativesharp::ast
{
class AstNode;
class TypeSyntax;
}

namespace nativesharp::frontend
{
// Provisional compiler selection. Body analysis must verify the declaration,
// owner and receiver before it creates an ordinary member binding.
struct StaticFieldAccessSelection final
{
    std::string declaringTypeIdentity;
    const ast::AstNode* fieldDeclaration = nullptr;
    // Exact visible nominal field type; empty for builtins.
    std::string projectedTypeIdentity;
    // Optional structural projection allocated in the owning syntax-unit arena.
    const ast::TypeSyntax* projectedTypeSyntax = nullptr;
};
}
