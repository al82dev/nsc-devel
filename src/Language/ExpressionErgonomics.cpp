#include "nativesharp/Language/ExpressionErgonomics.h"

#include <array>
#include <cctype>

namespace nativesharp::language
{
namespace
{
constexpr std::array<ExpressionLoweringContract, 7> kContracts = {{
    {ExpressionFeature::NamedArguments, "named arguments", true, true, false, true},
    {ExpressionFeature::TypedParameterPacks, "params ReadOnlySpan<T>", true, true, false, true},
    {ExpressionFeature::NullAwareOperators, "null-aware operators", true, true, false, true},
    {ExpressionFeature::RangeIndex, ".. / ^", true, true, false, true},
    {ExpressionFeature::GeneralizedInitializers, "generalized initializer", true, true, false, true},
    {ExpressionFeature::StaticInterpolation, "$\"...\"", true, true, false, true},
    {ExpressionFeature::WithExpressions, "with", true, true, false, true},
}};

std::string trim(std::string_view text)
{
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return std::string(text.substr(begin, end - begin));
}
}

const ExpressionLoweringContract& expressionLoweringContract(ExpressionFeature feature) noexcept
{
    return kContracts[static_cast<std::size_t>(feature)];
}

bool expressionFeatureAvailable(ExpressionFeature, LanguageEdition edition) noexcept
{
    return edition == LanguageEdition::V3;
}

bool isTypedParameterPackType(std::string_view typeKey) noexcept
{
    const std::size_t marker = typeKey.find("ReadOnlySpan<");
    return marker != std::string_view::npos && typeKey.ends_with('>');
}

NullAwareOperandKind classifyNullAwareOperand(std::string_view typeKey) noexcept
{
    if (typeKey.ends_with('*')) return NullAwareOperandKind::Pointer;
    if (typeKey.starts_with("type:") || typeKey == "builtin:string")
        return NullAwareOperandKind::Reference;
    if (typeKey.find("Optional<") != std::string_view::npos)
        return NullAwareOperandKind::StaticOptional;
    return NullAwareOperandKind::NotNullable;
}

bool supportsNullAwareOperators(std::string_view typeKey) noexcept
{
    return classifyNullAwareOperand(typeKey) != NullAwareOperandKind::NotNullable;
}

std::optional<std::size_t> resolveRangeEndpoint(
    RangeEndpoint endpoint, std::size_t length) noexcept
{
    if (!endpoint.fromEnd)
        return endpoint.value <= length ? std::optional(endpoint.value) : std::nullopt;
    return endpoint.value <= length ? std::optional(length - endpoint.value) : std::nullopt;
}

std::optional<ResolvedRange> resolveHalfOpenRange(
    std::optional<RangeEndpoint> start,
    std::optional<RangeEndpoint> end,
    std::size_t length) noexcept
{
    const std::optional<std::size_t> resolvedStart =
        start.has_value() ? resolveRangeEndpoint(*start, length) : std::optional<std::size_t>(0);
    const std::optional<std::size_t> resolvedEnd =
        end.has_value() ? resolveRangeEndpoint(*end, length) : std::optional<std::size_t>(length);
    if (!resolvedStart.has_value() || !resolvedEnd.has_value() || *resolvedStart > *resolvedEnd)
        return std::nullopt;
    return ResolvedRange{*resolvedStart, *resolvedEnd};
}

bool parseStaticInterpolation(
    std::string_view contents,
    std::vector<InterpolationPart>& parts,
    std::string& error)
{
    parts.clear();
    error.clear();
    std::string literal;
    const auto flushLiteral = [&]
    {
        if (literal.empty()) return;
        parts.push_back({InterpolationPartKind::Literal, std::move(literal), {}});
        literal.clear();
    };

    std::size_t cursor = 0;
    while (cursor < contents.size())
    {
        if (contents[cursor] == '{' && cursor + 1 < contents.size() && contents[cursor + 1] == '{')
        {
            literal.push_back('{');
            cursor += 2;
            continue;
        }
        if (contents[cursor] == '}' && cursor + 1 < contents.size() && contents[cursor + 1] == '}')
        {
            literal.push_back('}');
            cursor += 2;
            continue;
        }
        if (contents[cursor] == '}')
        {
            error = "unmatched '}' in interpolated string";
            parts.clear();
            return false;
        }
        if (contents[cursor] != '{')
        {
            literal.push_back(contents[cursor++]);
            continue;
        }

        flushLiteral();
        const std::size_t holeBegin = ++cursor;
        std::size_t nesting = 0;
        std::size_t formatSeparator = std::string_view::npos;
        char quote = 0;
        while (cursor < contents.size())
        {
            const char value = contents[cursor];
            if (quote != 0)
            {
                if (value == quote) quote = 0;
                ++cursor;
                continue;
            }
            if (value == '\'' || value == '"')
            {
                quote = value;
                ++cursor;
                continue;
            }
            if (value == '(' || value == '[' || value == '{')
            {
                ++nesting;
                ++cursor;
                continue;
            }
            if (value == ')' || value == ']')
            {
                if (nesting != 0) --nesting;
                ++cursor;
                continue;
            }
            if (value == '}' && nesting != 0)
            {
                --nesting;
                ++cursor;
                continue;
            }
            if (value == ':' && nesting == 0 && formatSeparator == std::string_view::npos)
                formatSeparator = cursor;
            if (value == '}' && nesting == 0) break;
            ++cursor;
        }
        if (cursor == contents.size())
        {
            error = "unterminated interpolation hole";
            parts.clear();
            return false;
        }
        const std::size_t expressionEnd =
            formatSeparator == std::string_view::npos ? cursor : formatSeparator;
        std::string expression = trim(contents.substr(holeBegin, expressionEnd - holeBegin));
        if (expression.empty())
        {
            error = "interpolation hole requires an expression";
            parts.clear();
            return false;
        }
        std::string format;
        if (formatSeparator != std::string_view::npos)
            format = std::string(contents.substr(formatSeparator + 1, cursor - formatSeparator - 1));
        parts.push_back({InterpolationPartKind::Hole, std::move(expression), std::move(format)});
        ++cursor;
    }
    flushLiteral();
    return true;
}
}
