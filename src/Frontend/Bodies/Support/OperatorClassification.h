#pragma once

#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Lexer/TokenKind.h"

namespace nativesharp::frontend
{
bool isComparison(lexer::TokenKind kind) noexcept;
bool isLogical(lexer::TokenKind kind) noexcept;
bool isArithmetic(lexer::TokenKind kind) noexcept;
bool isShift(lexer::TokenKind kind) noexcept;
std::string shiftResultType(std::string_view leftTypeKey);
std::string stage38OperatorMethodName(lexer::TokenKind kind, std::size_t arity);
std::string stage38OperatorTraitKey(lexer::TokenKind kind, std::size_t arity);
}
