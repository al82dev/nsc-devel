#include "nativesharp/Frontend/ComptimeEngine.h"
#include "nativesharp/Basic/Sha256.h"

#include <algorithm>
#include <charconv>
#include <limits>
#include <optional>
#include <span>

namespace nativesharp::frontend
{
namespace
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

class IntegerExpression final
{
public:
    IntegerExpression(
        std::string_view text,
        std::size_t maximumSteps,
        std::size_t maximumNesting,
        std::size_t& steps)
        : text_(text), maximumSteps_(maximumSteps), maximumNesting_(maximumNesting), steps_(steps)
    {
    }

    std::optional<std::int64_t> evaluate(std::string& error)
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

private:
    bool tick(std::string& error)
    {
        if (steps_ >= maximumSteps_)
        {
            error = "comptime step budget exceeded";
            return false;
        }
        ++steps_;
        return true;
    }

    void skipSpace()
    {
        while (cursor_ < text_.size() && asciiSpace(text_[cursor_]))
            ++cursor_;
    }

    bool take(std::string_view token)
    {
        skipSpace();
        if (text_.substr(cursor_, token.size()) != token) return false;
        cursor_ += token.size();
        return true;
    }

    std::optional<std::int64_t> parsePrimary(std::string& error)
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

    std::optional<std::int64_t> parseUnary(std::string& error)
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

    std::optional<std::int64_t> parseMultiply(std::string& error)
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

    std::optional<std::int64_t> parseAdd(std::string& error)
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

    std::optional<std::int64_t> parseShift(std::string& error)
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

    std::optional<std::int64_t> parseBitwiseAnd(std::string& error)
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

    std::optional<std::int64_t> parseBitwiseXor(std::string& error)
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

    std::optional<std::int64_t> parseBitwiseOr(std::string& error)
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

    std::string_view text_;
    std::size_t cursor_ = 0;
    std::size_t maximumSteps_ = 0;
    std::size_t maximumNesting_ = 0;
    std::size_t& steps_;
    std::size_t nesting_ = 0;
};

bool startsWord(std::string_view source, std::size_t offset, std::string_view word)
{
    if (source.substr(offset, word.size()) != word) return false;
    if (offset != 0 && identifierByte(source[offset - 1])) return false;
    const std::size_t after = offset + word.size();
    return after >= source.size() || !identifierByte(source[after]);
}

std::string comptimeCacheKey(std::string_view expression)
{
    std::string payload = "nativesharp-comptime-expression-v1\n";
    payload.append(expression);
    const auto bytes = std::span(
        reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size());
    return basic::sha256Hex(basic::sha256(bytes));
}

std::string reproducibilityDigestOf(const ComptimeResult& result)
{
    std::string payload = "nativesharp-comptime-result-v1\n";
    payload.append(result.source);
    payload.push_back('\n');
    for (std::size_t index = 0; index < result.dependencies.size(); ++index)
    {
        const ComptimeDependency& dependency = result.dependencies[index];
        payload.append(dependency.kind).push_back(':');
        payload.append(dependency.digest).push_back(':');
        payload.append(std::to_string(result.evaluations[index].value)).push_back('\n');
    }
    const auto bytes = std::span(
        reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size());
    return basic::sha256Hex(basic::sha256(bytes));
}
}

std::optional<ComptimeCache::Entry> ComptimeCache::find(std::string_view key) const
{
    std::lock_guard lock(mutex_);
    const auto found = entries_.find(std::string(key));
    return found == entries_.end() ? std::nullopt : std::optional<Entry>(found->second);
}

void ComptimeCache::store(std::string key, Entry entry)
{
    std::lock_guard lock(mutex_);
    entries_.insert_or_assign(std::move(key), entry);
}

std::size_t ComptimeCache::size() const
{
    std::lock_guard lock(mutex_);
    return entries_.size();
}

ComptimeEngine::ComptimeEngine(
    ComptimeLimits limits,
    std::shared_ptr<ComptimeCache> cache) noexcept
    : limits_(limits), cache_(std::move(cache))
{
}

ComptimeResult ComptimeEngine::execute(std::string_view source) const
{
    ComptimeResult result;
    if (source.size() > limits_.maximumSourceBytes)
    {
        result.error = "comptime source byte limit exceeded";
        return result;
    }
    result.source.assign(source);
    enum class Scan { Code, LineComment, BlockComment, String, Character } state = Scan::Code;
    bool escaped = false;
    for (std::size_t cursor = 0; cursor < source.size(); ++cursor)
    {
        const char current = source[cursor];
        const char next = cursor + 1 < source.size() ? source[cursor + 1] : '\0';
        if (state == Scan::LineComment)
        {
            if (current == '\n') state = Scan::Code;
            continue;
        }
        if (state == Scan::BlockComment)
        {
            if (current == '*' && next == '/') { state = Scan::Code; ++cursor; }
            continue;
        }
        if (state == Scan::String || state == Scan::Character)
        {
            if (escaped) { escaped = false; continue; }
            if (current == '\\') { escaped = true; continue; }
            if ((state == Scan::String && current == '"') ||
                (state == Scan::Character && current == '\'')) state = Scan::Code;
            continue;
        }
        if (current == '/' && next == '/') { state = Scan::LineComment; ++cursor; continue; }
        if (current == '/' && next == '*') { state = Scan::BlockComment; ++cursor; continue; }
        if (current == '"') { state = Scan::String; continue; }
        if (current == '\'') { state = Scan::Character; continue; }
        if (startsWord(source, cursor, "comptime"))
        {
            result.source.replace(cursor, 8, 8, ' ');
            cursor += 7;
            continue;
        }
        if (source.substr(cursor, 4) != "#run" ||
            (cursor != 0 && identifierByte(source[cursor - 1]))) continue;
        if (result.evaluations.size() >= limits_.maximumDirectives)
        {
            result.error = "comptime directive limit exceeded";
            return result;
        }
        std::size_t open = cursor + 4;
        while (open < source.size() && asciiSpace(source[open])) ++open;
        if (open == source.size() || source[open] != '(')
        {
            result.error = "expected '(' after #run at byte " + std::to_string(cursor);
            return result;
        }
        std::size_t close = open + 1;
        unsigned depth = 1;
        for (; close < source.size() && depth != 0; ++close)
        {
            if (source[close] == '(') ++depth;
            else if (source[close] == ')') --depth;
        }
        if (depth != 0)
        {
            result.error = "unterminated #run expression at byte " + std::to_string(cursor);
            return result;
        }
        const std::size_t end = close;
        const std::string_view expression = source.substr(open + 1, end - open - 2);
        const std::string dependencyDigest = comptimeCacheKey(expression);
        result.dependencies.push_back({
            "source-expression",
            "#run@" + std::to_string(cursor),
            dependencyDigest});
        std::string error;
        std::optional<std::int64_t> value;
        bool cacheHit = false;
        if (cache_)
        {
            if (const auto cached = cache_->find(dependencyDigest); cached.has_value())
            {
                if (cached->executedSteps > limits_.maximumSteps -
                    std::min(result.executedSteps, limits_.maximumSteps))
                {
                    result.error = "comptime step budget exceeded";
                    return result;
                }
                result.executedSteps += cached->executedSteps;
                value = cached->value;
                cacheHit = true;
                ++result.cacheHits;
            }
        }
        if (!value.has_value())
        {
            const std::size_t stepsBefore = result.executedSteps;
            IntegerExpression evaluator(expression,
                limits_.maximumSteps, limits_.maximumNesting, result.executedSteps);
            value = evaluator.evaluate(error);
            ++result.cacheMisses;
            if (value.has_value() && cache_)
                cache_->store(dependencyDigest, {*value, result.executedSteps - stepsBefore});
        }
        if (!value.has_value())
        {
            result.error = error + " (directive at byte " + std::to_string(cursor) + ")";
            return result;
        }
        const std::string replacement = std::to_string(*value);
        const std::size_t length = end - cursor;
        if (replacement.size() > length)
        {
            result.error = "#run result does not fit its source span at byte " +
                std::to_string(cursor);
            return result;
        }
        std::string padded = replacement;
        padded.append(length - replacement.size(), ' ');
        result.source.replace(cursor, length, padded);
        result.evaluations.push_back({cursor, length, *value, dependencyDigest, cacheHit});
        cursor = end - 1;
    }
    result.success = true;
    result.reproducibilityDigest = reproducibilityDigestOf(result);
    return result;
}
}
