#pragma once

// This narrows candidates only. It cannot by itself establish Verified or
// grant any permission to a consumer; semantic and lowering proof is pending.
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Lexer/TokenKind.h"
#include <string_view>

namespace nativesharp::frontend::view_producer_prototype
{
inline bool directTransferShape(const ast::AstNode* body,
                                std::string_view tupleParameter,
                                std::string_view outputParameter)
{
    if (body == nullptr || body->kind() != ast::AstNodeKind::BlockStatement ||
        body->children().size() != 2) return false;
    const auto* declaration = body->children()[0];
    const auto* statement = body->children()[1];
    if (declaration == nullptr || statement == nullptr ||
        declaration->kind() != ast::AstNodeKind::DeconstructionDeclarationStatement ||
        statement->kind() != ast::AstNodeKind::ExpressionStatement ||
        declaration->first() == nullptr ||
        declaration->first()->kind() != ast::AstNodeKind::IdentifierExpression ||
        declaration->first()->text() != tupleParameter) return false;
    const auto* assignment = statement->first();
    if (assignment == nullptr || assignment->kind() != ast::AstNodeKind::AssignmentExpression ||
        assignment->tokenKind() != lexer::TokenKind::Equal || assignment->first() == nullptr ||
        assignment->second() == nullptr ||
        assignment->first()->kind() != ast::AstNodeKind::IdentifierExpression ||
        assignment->second()->kind() != ast::AstNodeKind::IdentifierExpression ||
        assignment->first()->text() != outputParameter) return false;
    for (const auto* element : declaration->children())
        if (element != nullptr && element->kind() == ast::AstNodeKind::IdentifierExpression &&
            element->text() != "_" && element->text() == assignment->second()->text()) return true;
    return false;
}
}
