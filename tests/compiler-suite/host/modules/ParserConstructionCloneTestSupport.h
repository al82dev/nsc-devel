#pragma once

#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace nsc::suite::parser_clone_test_support
{
// These wrappers are defined by ParserProductTestShim.cpp, in the same translation unit as the
// product parser body. They expose anonymous-namespace helpers to directed tests without adding a
// symbol, branch, or test hook to the compiler product.
nativesharp::ast::TypeSyntax* cloneParsedType(
    nativesharp::ast::AstArena& arena,
    const nativesharp::ast::TypeSyntax* source);
nativesharp::ast::AstNode* cloneParsedNode(
    nativesharp::ast::AstArena& arena,
    const nativesharp::ast::AstNode* source);
std::string_view operatorSpelling(nativesharp::lexer::TokenKind kind) noexcept;
std::string operatorMethodName(nativesharp::lexer::TokenKind kind, std::size_t arity);
}
