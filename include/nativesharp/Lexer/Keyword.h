#pragma once

#include "nativesharp/Lexer/TokenKind.h"

#include <string_view>

namespace nativesharp::lexer
{
// Returns the token kind assigned by the lexer to an exact keyword spelling.
// Identifier is the stable result for names that remain available to declarations.
TokenKind keywordKind(std::string_view text) noexcept;
}
