#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace nativesharp::frontend::internal
{
bool asciiSpace(char value) noexcept;
bool asciiDigit(char value) noexcept;
bool asciiAlpha(char value) noexcept;
bool asciiAlnum(char value) noexcept;
bool identifierByte(char value) noexcept;

class IntegerExpression final
{
public:
    IntegerExpression(
        std::string_view text,
        std::size_t maximumSteps,
        std::size_t maximumNesting,
        std::size_t& steps,
        const std::function<bool()>& cancellationRequested);

    std::optional<std::int64_t> evaluate(std::string& error);

private:
    bool tick(std::string& error);

    void skipSpace();

    bool take(std::string_view token);

    std::optional<std::int64_t> parsePrimary(std::string& error);

    std::optional<std::int64_t> parseUnary(std::string& error);

    std::optional<std::int64_t> parseMultiply(std::string& error);

    std::optional<std::int64_t> parseAdd(std::string& error);

    std::optional<std::int64_t> parseShift(std::string& error);

    std::optional<std::int64_t> parseBitwiseAnd(std::string& error);

    std::optional<std::int64_t> parseBitwiseXor(std::string& error);

    std::optional<std::int64_t> parseBitwiseOr(std::string& error);

    std::string_view text_;
    std::size_t cursor_ = 0;
    std::size_t maximumSteps_ = 0;
    std::size_t maximumNesting_ = 0;
    std::size_t& steps_;
    std::size_t nesting_ = 0;
    const std::function<bool()>& cancellationRequested_;
};
}
