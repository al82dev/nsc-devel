#include "IntegerExpression.h"

#include <charconv>
#include <limits>
#include <utility>

namespace nativesharp::frontend::internal
{
bool asciiSpace(char value) noexcept
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' ||
        value == '\f' || value == '\v';
}

bool asciiDigit(char value) noexcept { return value >= '0' && value <= '9'; }

bool asciiAlpha(char value) noexcept
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

bool asciiAlnum(char value) noexcept { return asciiAlpha(value) || asciiDigit(value); }

bool identifierByte(char value) noexcept
{
    return asciiAlnum(value) || value == '_';
}

IntegerExpression::IntegerExpression(
        std::string_view text,
        std::size_t maximumSteps,
        std::size_t maximumNesting,
        std::size_t& steps,
        const std::function<bool()>& cancellationRequested)
        : text_(text), maximumSteps_(maximumSteps), maximumNesting_(maximumNesting), steps_(steps),
          cancellationRequested_(cancellationRequested)
    {
    }

std::optional<std::int64_t> IntegerExpression::evaluate(std::string& error)
    {
        const auto value = parseBitwiseOr(error);
        skipSpace();
        if (value.has_value() && cursor_ != text_.size())
        {
            error = "unexpected token in #run expression at byte " + std::to_string(cursor_);
            return std::nullopt;
        }
        return value;
    }

bool IntegerExpression::tick(std::string& error)
    {
        if (cancellationRequested_ && cancellationRequested_())
        {
            error = "comptime evaluation cancelled";
            return false;
        }
        if (steps_ >= maximumSteps_)
        {
            error = "comptime step budget exceeded";
            return false;
        }
        ++steps_;
        return true;
    }

void IntegerExpression::skipSpace()
    {
        while (cursor_ < text_.size() && asciiSpace(text_[cursor_]))
            ++cursor_;
    }

bool IntegerExpression::take(std::string_view token)
    {
        skipSpace();
        if (text_.substr(cursor_, token.size()) != token) return false;
        cursor_ += token.size();
        return true;
    }

std::optional<std::int64_t> IntegerExpression::parsePrimary(std::string& error)
    {
        if (!tick(error)) return std::nullopt;
        skipSpace();
        if (take("("))
        {
            if (nesting_ >= maximumNesting_)
            {
                error = "comptime nesting limit exceeded";
                return std::nullopt;
            }
            ++nesting_;
            auto value = parseBitwiseOr(error);
            --nesting_;
            if (!value.has_value() || !take(")"))
            {
                if (error.empty()) error = "expected ')' in #run expression";
                return std::nullopt;
            }
            return value;
        }
        const std::size_t begin = cursor_;
        if (cursor_ < text_.size() &&
            (asciiAlpha(text_[cursor_]) ||
             text_[cursor_] == '_'))
        {
            error = "comptime sandbox forbids identifiers, calls, FFI, and host access";
            return std::nullopt;
        }
        int base = 10;
        if (cursor_ + 2 <= text_.size() && text_[cursor_] == '0' &&
            (text_[cursor_ + 1] == 'x' || text_[cursor_ + 1] == 'X'))
        {
            base = 16;
            cursor_ += 2;
        }
        const std::size_t digits = cursor_;
        while (cursor_ < text_.size() &&
               (asciiAlnum(text_[cursor_]) ||
                text_[cursor_] == '_'))
            ++cursor_;
        if (cursor_ == digits)
        {
            error = "expected integer expression after #run(";
            return std::nullopt;
        }
        std::string clean;
        for (char value : text_.substr(digits, cursor_ - digits))
            if (value != '_') clean.push_back(value);
        std::uint64_t parsed = 0;
        const auto converted = std::from_chars(clean.data(), clean.data() + clean.size(), parsed, base);
        if (converted.ec != std::errc{} || converted.ptr != clean.data() + clean.size() ||
            parsed > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        {
            error = "invalid signed 64-bit integer in #run expression at byte " +
                std::to_string(begin);
            return std::nullopt;
        }
        return static_cast<std::int64_t>(parsed);
    }

std::optional<std::int64_t> IntegerExpression::parseUnary(std::string& error)
    {
        if (!tick(error)) return std::nullopt;
        if (take("+")) return parseUnary(error);
        if (take("-"))
        {
            auto value = parseUnary(error);
            if (!value.has_value()) return std::nullopt;
            if (*value == std::numeric_limits<std::int64_t>::min())
            {
                error = "signed overflow in #run unary '-'";
                return std::nullopt;
            }
            return -*value;
        }
        if (take("~"))
        {
            auto value = parseUnary(error);
            return value.has_value() ? std::optional<std::int64_t>(~*value) : std::nullopt;
        }
        return parsePrimary(error);
    }

std::optional<std::int64_t> IntegerExpression::parseMultiply(std::string& error)
    {
        if (!tick(error)) return std::nullopt;
        auto left = parseUnary(error);
        while (left.has_value())
        {
            char op = 0;
            if (take("*")) op = '*';
            else if (take("/")) op = '/';
            else if (take("%")) op = '%';
            else break;
            auto right = parseUnary(error);
            if (!right.has_value()) return std::nullopt;
            if ((op == '/' || op == '%') && *right == 0)
            {
                error = "division by zero in #run expression";
                return std::nullopt;
            }
            if ((op == '/' || op == '%') &&
                *left == std::numeric_limits<std::int64_t>::min() && *right == -1)
            {
                error = "signed overflow in #run expression";
                return std::nullopt;
            }
            if (op == '*')
            {
                std::int64_t product = 0;
                if (!__builtin_mul_overflow(*left, *right, &product)) left = product;
                else { error = "signed overflow in #run expression"; return std::nullopt; }
            }
            else if (op == '/') left = *left / *right;
            else left = *left % *right;
        }
        return left;
    }

std::optional<std::int64_t> IntegerExpression::parseAdd(std::string& error)
    {
        if (!tick(error)) return std::nullopt;
        auto left = parseMultiply(error);
        while (left.has_value())
        {
            char op = 0;
            if (take("+")) op = '+';
            else if (take("-")) op = '-';
            else break;
            auto right = parseMultiply(error);
            if (!right.has_value()) return std::nullopt;
            std::int64_t result = 0;
            const bool overflow = op == '+' ? __builtin_add_overflow(*left, *right, &result)
                                            : __builtin_sub_overflow(*left, *right, &result);
            if (overflow) { error = "signed overflow in #run expression"; return std::nullopt; }
            left = result;
        }
        return left;
    }

std::optional<std::int64_t> IntegerExpression::parseShift(std::string& error)
    {
        if (!tick(error)) return std::nullopt;
        auto left = parseAdd(error);
        while (left.has_value())
        {
            bool rightShift = false;
            if (take("<<")) {}
            else if (take(">>")) rightShift = true;
            else break;
            auto right = parseAdd(error);
            if (!right.has_value()) return std::nullopt;
            if (*right < 0 || *right >= 64)
            {
                error = "shift count outside 0..63 in #run expression";
                return std::nullopt;
            }
            const auto bits = static_cast<std::uint64_t>(*left);
            left = static_cast<std::int64_t>(rightShift ? bits >> *right : bits << *right);
        }
        return left;
    }

std::optional<std::int64_t> IntegerExpression::parseBitwiseAnd(std::string& error)
    {
        if (!tick(error)) return std::nullopt;
        auto value = parseShift(error);
        while (value.has_value() && take("&"))
        {
            auto right = parseShift(error);
            if (!right.has_value()) return std::nullopt;
            value = *value & *right;
        }
        return value;
    }

std::optional<std::int64_t> IntegerExpression::parseBitwiseXor(std::string& error)
    {
        if (!tick(error)) return std::nullopt;
        auto value = parseBitwiseAnd(error);
        while (value.has_value() && take("^"))
        {
            auto right = parseBitwiseAnd(error);
            if (!right.has_value()) return std::nullopt;
            value = *value ^ *right;
        }
        return value;
    }

std::optional<std::int64_t> IntegerExpression::parseBitwiseOr(std::string& error)
    {
        if (!tick(error)) return std::nullopt;
        auto value = parseBitwiseXor(error);
        while (value.has_value() && take("|"))
        {
            auto right = parseBitwiseXor(error);
            if (!right.has_value()) return std::nullopt;
            value = *value | *right;
        }
        return value;
    }
}
