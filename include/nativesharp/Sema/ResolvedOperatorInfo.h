#pragma once

#include "nativesharp/Lexer/TokenKind.h"

namespace nativesharp::sema
{
class Type;

struct ResolvedOperatorInfo
{
    lexer::TokenKind tokenKind = lexer::TokenKind::Invalid;
    Type* leftType = nullptr;
    Type* rightType = nullptr;
    Type* operationType = nullptr;
    Type* resultType = nullptr;
    bool unary = false;
};
}
