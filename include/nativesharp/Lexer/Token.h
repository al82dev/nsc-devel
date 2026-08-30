#pragma once

#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

namespace nativesharp::lexer
{
using TokenValue = std::variant<std::monostate, std::uint64_t, double, std::uint8_t, std::string>;

class Token final
{
public:
    Token(TokenKind kind, SourceRange range, std::string_view lexeme, TokenValue value = {});

    TokenKind kind() const noexcept;
    SourceRange range() const noexcept;
    std::string_view lexeme() const noexcept;
    const TokenValue& value() const noexcept;

private:
    TokenKind kind_;
    SourceRange range_;
    std::string_view lexeme_;
    TokenValue value_;
};
}
