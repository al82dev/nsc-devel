#include "ViewProducerBodyPrototype.h"
#include <cassert>

using namespace nativesharp;
using namespace nativesharp::frontend::view_producer_prototype;

int main()
{
    ast::AstNode body(ast::AstNodeKind::BlockStatement, {});
    ast::AstNode unpack(ast::AstNodeKind::DeconstructionDeclarationStatement, {});
    ast::AstNode pair(ast::AstNodeKind::IdentifierExpression, {});
    pair.setText("pair");
    unpack.setFirst(&pair);
    ast::AstNode view(ast::AstNodeKind::IdentifierExpression, {});
    view.setText("view");
    unpack.addChild(&view);
    ast::AstNode number(ast::AstNodeKind::IdentifierExpression, {});
    number.setText("number");
    unpack.addChild(&number);
    ast::AstNode statement(ast::AstNodeKind::ExpressionStatement, {});
    ast::AstNode assignment(ast::AstNodeKind::AssignmentExpression, {});
    assignment.setTokenKind(lexer::TokenKind::Equal);
    ast::AstNode output(ast::AstNodeKind::IdentifierExpression, {});
    output.setText("output");
    assignment.setFirst(&output);
    assignment.setSecond(&view);
    statement.setFirst(&assignment);
    body.addChild(&unpack);
    body.addChild(&statement);
    assert(directTransferShape(&body, "pair", "output"));
    view.setText("other");
    ast::AstNode independent(ast::AstNodeKind::IdentifierExpression, {});
    independent.setText("unrelated");
    assignment.setSecond(&independent);
    assert(!directTransferShape(&body, "pair", "output"));
    assignment.setSecond(&view);
    ast::AstNode call(ast::AstNodeKind::CallExpression, {});
    assignment.setSecond(&call);
    assert(!directTransferShape(&body, "pair", "output"));
    assignment.setSecond(&view);
    assignment.setTokenKind(lexer::TokenKind::PlusEqual);
    assert(!directTransferShape(&body, "pair", "output"));
    assignment.setTokenKind(lexer::TokenKind::Equal);
    ast::AstNode extra(ast::AstNodeKind::ExpressionStatement, {});
    body.addChild(&extra);
    assert(!directTransferShape(&body, "pair", "output"));
}
