#include "ExpressionParsingSupport.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"

#include <limits>

namespace nativesharp::parser
{
using lexer::TokenKind;

std::optional<std::int64_t> signedIntegerLiteral(const ast::AstNode* expression) noexcept
{
    if (expression == nullptr) return std::nullopt;
    if (expression->kind() == ast::AstNodeKind::IntegerLiteralExpression)
    {
        if (expression->integerValue() >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
            return std::nullopt;
        return static_cast<std::int64_t>(expression->integerValue());
    }
    if (expression->kind() != ast::AstNodeKind::UnaryExpression ||
        expression->tokenKind() != TokenKind::Minus || expression->first() == nullptr ||
        expression->first()->kind() != ast::AstNodeKind::IntegerLiteralExpression)
        return std::nullopt;
    const std::uint64_t magnitude = expression->first()->integerValue();
    if (magnitude > (std::uint64_t{1} << 63U)) return std::nullopt;
    if (magnitude == (std::uint64_t{1} << 63U))
        return std::numeric_limits<std::int64_t>::min();
    return -static_cast<std::int64_t>(magnitude);
}

std::optional<BinaryOperatorInfo> binaryOperatorInfo(TokenKind kind) noexcept
{
    switch (kind)
    {
    case TokenKind::PipePipe:
        return BinaryOperatorInfo{
            5, 6, BinaryRightHandSide::Expression, ast::AstNodeKind::BinaryExpression};
    case TokenKind::AmpAmp:
        return BinaryOperatorInfo{
            6, 7, BinaryRightHandSide::Expression, ast::AstNodeKind::BinaryExpression};
    case TokenKind::Pipe:
        return BinaryOperatorInfo{
            7, 8, BinaryRightHandSide::Expression, ast::AstNodeKind::BinaryExpression};
    case TokenKind::Caret:
        return BinaryOperatorInfo{
            8, 9, BinaryRightHandSide::Expression, ast::AstNodeKind::BinaryExpression};
    case TokenKind::Amp:
        return BinaryOperatorInfo{
            9, 10, BinaryRightHandSide::Expression, ast::AstNodeKind::BinaryExpression};
    case TokenKind::EqualEqual:
    case TokenKind::BangEqual:
        return BinaryOperatorInfo{
            10, 11, BinaryRightHandSide::Expression, ast::AstNodeKind::BinaryExpression};
    case TokenKind::Less:
    case TokenKind::LessEqual:
    case TokenKind::Greater:
    case TokenKind::GreaterEqual:
        return BinaryOperatorInfo{
            11, 12, BinaryRightHandSide::Expression, ast::AstNodeKind::BinaryExpression};
    case TokenKind::KwIs:
        return BinaryOperatorInfo{
            11, 12, BinaryRightHandSide::Type, ast::AstNodeKind::TypeTestExpression};
    case TokenKind::KwAs:
        return BinaryOperatorInfo{
            11, 12, BinaryRightHandSide::Type, ast::AstNodeKind::SafeCastExpression};
    case TokenKind::LessLess:
    case TokenKind::GreaterGreater:
        return BinaryOperatorInfo{
            12, 13, BinaryRightHandSide::Expression, ast::AstNodeKind::BinaryExpression};
    case TokenKind::Plus:
    case TokenKind::Minus:
        return BinaryOperatorInfo{
            13, 14, BinaryRightHandSide::Expression, ast::AstNodeKind::BinaryExpression};
    case TokenKind::Star:
    case TokenKind::Slash:
    case TokenKind::Percent:
        return BinaryOperatorInfo{
            14, 15, BinaryRightHandSide::Expression, ast::AstNodeKind::BinaryExpression};
    default: return std::nullopt;
    }
}

std::uint8_t expressionBindingPower(TokenKind kind) noexcept
{
    if (lexer::isAssignmentOperator(kind)) return 1;
    if (kind == TokenKind::Question) return 2;
    if (kind == TokenKind::QuestionQuestion) return 3;
    if (kind == TokenKind::DotDot || kind == TokenKind::DotDotEqual) return 4;
    const std::optional<BinaryOperatorInfo> binary = binaryOperatorInfo(kind);
    return binary.has_value() ? binary->leftBindingPower : 0;
}

bool postfixBridgeRightAssociative(TokenKind kind) noexcept
{
    return lexer::isAssignmentOperator(kind) || kind == TokenKind::QuestionQuestion;
}

bool rangeOperandEnds(TokenKind kind) noexcept
{
    return kind == TokenKind::EndOfFile || kind == TokenKind::RightParen ||
        kind == TokenKind::RightBracket || kind == TokenKind::Comma ||
        kind == TokenKind::Semicolon;
}
}
