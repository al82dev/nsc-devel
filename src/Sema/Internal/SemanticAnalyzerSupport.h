#pragma once

#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Sema/ExpressionInfo.h"

#include <bit>
#include <cstdint>
#include <limits>

namespace nativesharp::sema::semantic_analyzer
{
inline lexer::TokenKind underlyingBinaryOperator(lexer::TokenKind kind) noexcept
{
    using lexer::TokenKind;
    switch (kind)
    {
        case TokenKind::PlusEqual: return TokenKind::Plus;
        case TokenKind::MinusEqual: return TokenKind::Minus;
        case TokenKind::StarEqual: return TokenKind::Star;
        case TokenKind::SlashEqual: return TokenKind::Slash;
        case TokenKind::PercentEqual: return TokenKind::Percent;
        case TokenKind::AmpEqual: return TokenKind::Amp;
        case TokenKind::PipeEqual: return TokenKind::Pipe;
        case TokenKind::CaretEqual: return TokenKind::Caret;
        case TokenKind::LessLessEqual: return TokenKind::LessLess;
        case TokenKind::GreaterGreaterEqual: return TokenKind::GreaterGreater;
        default: return kind;
    }
}

inline std::uint64_t maskForWidth(std::uint32_t width) noexcept
{
    return width >= 64 ? std::numeric_limits<std::uint64_t>::max() : ((std::uint64_t{1} << width) - 1);
}

inline std::int64_t signedFromBits(std::uint64_t bits, std::uint32_t width) noexcept
{
    if (width >= 64)
    {
        return std::bit_cast<std::int64_t>(bits);
    }

    const std::uint64_t mask = maskForWidth(width);
    bits &= mask;
    const std::uint64_t sign = std::uint64_t{1} << (width - 1);
    if ((bits & sign) == 0)
    {
        return static_cast<std::int64_t>(bits);
    }

    const std::uint64_t magnitude = ((~bits) + 1) & mask;
    return -static_cast<std::int64_t>(magnitude);
}

inline std::uint64_t bitsFromSigned(std::int64_t value, std::uint32_t width) noexcept
{
    return std::bit_cast<std::uint64_t>(value) & maskForWidth(width);
}

inline bool isNegativeConstant(const ExpressionInfo& info) noexcept
{
    return info.isConstant && info.constantValue.kind() == ConstantKind::SignedInteger &&
        info.constantValue.signedInteger() < 0;
}
}
