#include "BuiltinStaticInterpolationHandler.h"

#include "ParsedBuiltinHole.h"

#include <charconv>
#include <cctype>
#include <system_error>

namespace nativesharp::language
{
namespace
{
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
}

BuiltinStaticInterpolationHandler::BuiltinStaticInterpolationHandler(
    std::string& result) noexcept : result_(result) {}

bool BuiltinStaticInterpolationHandler::appendLiteral(
    std::string_view text,
    std::string&)
{
    result_.append(text);
    return true;
}

bool BuiltinStaticInterpolationHandler::appendHole(
    const InterpolationPart& hole,
    std::string& error)
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
}
