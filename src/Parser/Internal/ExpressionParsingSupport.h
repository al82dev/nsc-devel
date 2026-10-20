#pragma once

#include "BinaryOperatorInfo.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <cstdint>
#include <optional>

namespace nativesharp::ast
{
class AstNode;
}

namespace nativesharp::parser
{
std::optional<std::int64_t> signedIntegerLiteral(const ast::AstNode* expression) noexcept;
std::optional<BinaryOperatorInfo> binaryOperatorInfo(lexer::TokenKind kind) noexcept;
std::uint8_t expressionBindingPower(lexer::TokenKind kind) noexcept;
bool postfixBridgeRightAssociative(lexer::TokenKind kind) noexcept;
bool rangeOperandEnds(lexer::TokenKind kind) noexcept;
}
