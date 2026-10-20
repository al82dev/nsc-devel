#pragma once

#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Lexer/TokenKind.h"

namespace nativesharp::frontend
{
bool isComparison(TokenKind kind) noexcept;
bool isLogical(TokenKind kind) noexcept;
bool isArithmetic(TokenKind kind) noexcept;
bool isShift(TokenKind kind) noexcept;
std::string shiftResultType(std::string_view leftTypeKey);
std::string stage38OperatorMethodName(TokenKind kind, std::size_t arity);
std::string stage38OperatorTraitKey(TokenKind kind, std::size_t arity);
}
