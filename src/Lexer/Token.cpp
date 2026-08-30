#include "nativesharp/Lexer/Token.h"

#include <utility>

namespace nativesharp::lexer
{
Token::Token(TokenKind kind, SourceRange range, std::string_view lexeme, TokenValue value)
    : kind_(kind),
      range_(range),
      lexeme_(lexeme),
      value_(std::move(value))
{
}

TokenKind Token::kind() const noexcept
{
    return kind_;
}

SourceRange Token::range() const noexcept
{
    return range_;
}

std::string_view Token::lexeme() const noexcept
{
    return lexeme_;
}

const TokenValue& Token::value() const noexcept
{
    return value_;
}
}
