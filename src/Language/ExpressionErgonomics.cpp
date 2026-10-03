#include "nativesharp/Language/ExpressionErgonomics.h"

#include <array>
#include <charconv>
#include <cctype>
#include <system_error>

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
        parts.push_back({InterpolationPartKind::Literal, std::move(literal), {}, 0, 0});
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
        const std::string_view untrimmed = contents.substr(holeBegin, expressionEnd - holeBegin);
        std::size_t leading = 0;
        while (leading < untrimmed.size() &&
               std::isspace(static_cast<unsigned char>(untrimmed[leading])))
        {
            ++leading;
        }
        std::size_t trailing = untrimmed.size();
        while (trailing > leading &&
               std::isspace(static_cast<unsigned char>(untrimmed[trailing - 1])))
        {
            --trailing;
        }
        std::string expression(untrimmed.substr(leading, trailing - leading));
        if (expression.empty())
        {
            error = "interpolation hole requires an expression";
            parts.clear();
            return false;
        }
        std::string format;
        if (formatSeparator != std::string_view::npos)
            format = std::string(contents.substr(formatSeparator + 1, cursor - formatSeparator - 1));
        const std::size_t expressionLength = expression.size();
        parts.push_back({InterpolationPartKind::Hole, std::move(expression), std::move(format),
            holeBegin + leading, expressionLength});
        ++cursor;
    }
    flushLiteral();
    return true;
}

namespace
{
struct ParsedBuiltinHole final
{
    bool boolean = false;
    bool booleanValue = false;
    bool negative = false;
    std::uint64_t magnitude = 0;
    char mode = 'D';
    std::size_t width = 0;
};

bool builtinHoleSyntaxSupported(const InterpolationPart& part) noexcept
{
    if (part.kind != InterpolationPartKind::Hole) return false;
    if (part.text == "true" || part.text == "false")
        return part.format.empty();

    std::string_view digits = part.text;
    if (!digits.empty() && digits.front() == '-') digits.remove_prefix(1);
    if (digits.empty()) return false;
    for (const char digit : digits)
        if (!std::isdigit(static_cast<unsigned char>(digit))) return false;

    if (part.format.empty()) return true;
    const char mode = part.format.front();
    if (mode != 'D' && mode != 'd' && mode != 'X' && mode != 'x') return false;
    if (part.text.front() == '-' && (mode == 'X' || mode == 'x')) return false;

    const std::string_view widthText(part.format.data() + 1, part.format.size() - 1);
    if (widthText.empty()) return true;
    std::size_t width = 0;
    const auto widthResult = std::from_chars(
        widthText.data(), widthText.data() + widthText.size(), width, 10);
    return widthResult.ec == std::errc{} &&
        widthResult.ptr == widthText.data() + widthText.size() &&
        width <= 64U;
}

bool parseBuiltinHole(const InterpolationPart& part, ParsedBuiltinHole& parsed) noexcept
{
    if (!builtinHoleSyntaxSupported(part)) return false;
    if (part.text == "true" || part.text == "false")
    {
        parsed.boolean = true;
        parsed.booleanValue = part.text == "true";
        return true;
    }

    std::string_view digits = part.text;
    if (digits.front() == '-')
    {
        parsed.negative = true;
        digits.remove_prefix(1);
    }
    const auto number = std::from_chars(
        digits.data(), digits.data() + digits.size(), parsed.magnitude, 10);
    if (number.ec != std::errc{} || number.ptr != digits.data() + digits.size()) return false;

    if (!part.format.empty())
    {
        parsed.mode = part.format.front();
        const std::string_view widthText(part.format.data() + 1, part.format.size() - 1);
        if (!widthText.empty())
            (void)std::from_chars(
                widthText.data(), widthText.data() + widthText.size(), parsed.width, 10);
    }
    return true;
}

class BuiltinStaticInterpolationHandler final : public StaticInterpolationHandler
{
public:
    explicit BuiltinStaticInterpolationHandler(std::string& result) noexcept : result_(result) {}

    bool appendLiteral(std::string_view text, std::string&) override
    {
        result_.append(text);
        return true;
    }

    bool appendHole(const InterpolationPart& hole, std::string& error) override
    {
        ParsedBuiltinHole parsed;
        if (!parseBuiltinHole(hole, parsed))
        {
            error = "static interpolation hole '" + hole.text +
                "' must be a bool or integer literal with constant D/d/X/x format; "
                "runtime holes require an explicit formatting handler";
            return false;
        }
        if (parsed.boolean)
        {
            result_ += parsed.booleanValue ? "true" : "false";
            return true;
        }

        // The sign participates in decimal width, matching runtime ZFill: -42:D5 is -0042,
        // not -00042. Likewise, -0 is the integer value zero and never renders a phantom sign.
        if (parsed.magnitude == 0) parsed.negative = false;
        char buffer[96] = {};
        const int base = (parsed.mode == 'X' || parsed.mode == 'x') ? 16 : 10;
        const auto converted =
            std::to_chars(buffer, buffer + sizeof(buffer), parsed.magnitude, base);
        if (converted.ec != std::errc{})
        {
            error = "static interpolation could not render integer hole '" + hole.text + "'";
            return false;
        }
        std::string text(buffer, converted.ptr);
        if (parsed.mode == 'X')
        {
            for (char& value : text)
                value = static_cast<char>(std::toupper(static_cast<unsigned char>(value)));
        }
        const std::size_t digitWidth = parsed.negative && parsed.width != 0
            ? parsed.width - 1U : parsed.width;
        if (text.size() < digitWidth) text.insert(0, digitWidth - text.size(), '0');
        if (parsed.negative) text.insert(text.begin(), '-');
        result_ += text;
        return true;
    }

private:
    std::string& result_;
};
}

bool renderStaticInterpolation(
    std::span<const InterpolationPart> parts,
    StaticInterpolationHandler& handler,
    std::string& error)
{
    error.clear();
    for (const InterpolationPart& part : parts)
    {
        const bool accepted = part.kind == InterpolationPartKind::Literal
            ? handler.appendLiteral(part.text, error)
            : handler.appendHole(part, error);
        if (!accepted)
        {
            if (error.empty()) error = "compile-time interpolation handler rejected a part";
            return false;
        }
    }
    return true;
}

bool builtinStaticInterpolationHoleSupported(const InterpolationPart& part) noexcept
{
    if (part.kind != InterpolationPartKind::Hole) return true;
    return builtinHoleSyntaxSupported(part);
}

bool renderBuiltinStaticInterpolation(
    std::span<const InterpolationPart> parts,
    std::string& result,
    std::string& error)
{
    result.clear();
    BuiltinStaticInterpolationHandler handler(result);
    if (renderStaticInterpolation(parts, handler, error)) return true;
    result.clear();
    return false;
}
}
