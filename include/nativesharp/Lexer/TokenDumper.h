#pragma once

#include "nativesharp/Lexer/Token.h"

#include <iosfwd>
#include <span>

namespace nativesharp::lexer
{
class TokenDumper final
{
public:
    void dump(std::span<const Token> tokens, std::ostream& output) const;
};
}
