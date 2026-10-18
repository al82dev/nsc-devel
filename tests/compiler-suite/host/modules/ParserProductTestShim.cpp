#include "ParserFoundationTestSupport.h"
#include "ParserConstructionCloneTestSupport.h"

// Parser's canonical plan contains cursor and lookahead contracts without a public seam. The
// support header exposes that seam only to this test target before compiling the product body,
// keeping the access model consistent across Parser definitions and their directed tests.
#include "../../../../src/Parser/Parser.cpp"
#include "../../../../src/Parser/ParserContext.cpp"
#include "../../../../src/Parser/DeclarationParser.cpp"
#include "../../../../src/Parser/MemberParser.cpp"
#include "../../../../src/Parser/TypeParser.cpp"
#include "../../../../src/Parser/StatementParser.cpp"
#include "../../../../src/Parser/ExpressionParser.cpp"

namespace nsc::suite::parser_clone_test_support
{
nativesharp::ast::TypeSyntax* cloneParsedType(
    nativesharp::ast::AstArena& arena,
    const nativesharp::ast::TypeSyntax* source)
{
    return nativesharp::parser::cloneParsedType(arena, source);
}

nativesharp::ast::AstNode* cloneParsedNode(
    nativesharp::ast::AstArena& arena,
    const nativesharp::ast::AstNode* source)
{
    return nativesharp::parser::cloneParsedNode(arena, source);
}

std::string_view operatorSpelling(nativesharp::lexer::TokenKind kind) noexcept
{
    return nativesharp::parser::operatorSpelling(kind);
}

std::string operatorMethodName(nativesharp::lexer::TokenKind kind, std::size_t arity)
{
    return nativesharp::parser::operatorMethodName(kind, arity);
}
}
