#include "nativesharp/Language/NumericSemantics.h"

#include <charconv>
#include <limits>
#include <array>

namespace nativesharp::language
{
namespace
{
std::uint64_t unsignedMaximum(unsigned bits) noexcept
{
    return bits == 64 ? std::numeric_limits<std::uint64_t>::max() :
        (std::uint64_t{1} << bits) - 1U;
}

std::int64_t signedMinimum(unsigned bits) noexcept
{
    return bits == 64 ? std::numeric_limits<std::int64_t>::min() :
        -(std::int64_t{1} << (bits - 1U));
}

std::int64_t signedMaximum(unsigned bits) noexcept
{
    return bits == 64 ? std::numeric_limits<std::int64_t>::max() :
        (std::int64_t{1} << (bits - 1U)) - 1;
}
}

bool literalFits(std::uint64_t magnitude, bool negative, IntegerType type) noexcept
{
    if (type.bits == 0 || type.bits > 64) return false;
    if (!type.isSigned) return !negative && magnitude <= unsignedMaximum(type.bits);
    const std::uint64_t positive = static_cast<std::uint64_t>(signedMaximum(type.bits));
    return negative ? magnitude <= positive + 1U : magnitude <= positive;
}

std::optional<std::uint64_t> parseIntegerLiteralMagnitude(
    std::string_view spelling,
    std::string& error)
{
    error.clear();
    std::string clean;
    clean.reserve(spelling.size());
    for (char value : spelling) if (value != '_') clean.push_back(value);
    int base = 10;
    std::size_t prefix = 0;
    if (clean.starts_with("0x") || clean.starts_with("0X")) { base = 16; prefix = 2; }
    else if (clean.starts_with("0b") || clean.starts_with("0B")) { base = 2; prefix = 2; }
    std::uint64_t result = 0;
    const auto parsed = std::from_chars(clean.data() + prefix, clean.data() + clean.size(), result, base);
    if (parsed.ec != std::errc{} || parsed.ptr != clean.data() + clean.size())
    { error = "invalid or overflowing integer literal"; return std::nullopt; }
    return result;
}

std::optional<UntypedIntegerLiteral> parseUntypedIntegerLiteral(
    std::string_view spelling,
    std::string& error)
{
    error.clear();
    if (spelling.empty() || spelling.front() == '+' || spelling.front() == '-')
    {
        error = "integer literal spelling is unsigned; unary sign is a separate expression";
        return std::nullopt;
    }

    struct SuffixEntry final { std::string_view text; IntegerLiteralSuffix suffix; };
    static constexpr std::array suffixes = {
        SuffixEntry{"isize", IntegerLiteralSuffix::ISize},
        SuffixEntry{"usize", IntegerLiteralSuffix::USize},
        SuffixEntry{"i16", IntegerLiteralSuffix::I16},
        SuffixEntry{"i32", IntegerLiteralSuffix::I32},
        SuffixEntry{"i64", IntegerLiteralSuffix::I64},
        SuffixEntry{"u16", IntegerLiteralSuffix::U16},
        SuffixEntry{"u32", IntegerLiteralSuffix::U32},
        SuffixEntry{"u64", IntegerLiteralSuffix::U64},
        SuffixEntry{"i8", IntegerLiteralSuffix::I8},
        SuffixEntry{"u8", IntegerLiteralSuffix::U8}};

    IntegerLiteralSuffix suffix = IntegerLiteralSuffix::None;
    for (const SuffixEntry& candidate : suffixes)
    {
        if (spelling.ends_with(candidate.text))
        {
            suffix = candidate.suffix;
            spelling.remove_suffix(candidate.text.size());
            break;
        }
    }

    IntegerLiteralBase base = IntegerLiteralBase::Decimal;
    std::size_t prefix = 0;
    if (spelling.starts_with("0x") || spelling.starts_with("0X"))
    { base = IntegerLiteralBase::Hexadecimal; prefix = 2; }
    else if (spelling.starts_with("0b") || spelling.starts_with("0B"))
    { base = IntegerLiteralBase::Binary; prefix = 2; }
    else if (spelling.starts_with("0o") || spelling.starts_with("0O"))
    { base = IntegerLiteralBase::Octal; prefix = 2; }
    if (spelling.size() == prefix || spelling[prefix] == '_' || spelling.back() == '_')
    {
        error = "integer literal separators must appear between digits";
        return std::nullopt;
    }

    std::string digits;
    digits.reserve(spelling.size() - prefix);
    bool previousSeparator = false;
    for (std::size_t index = prefix; index < spelling.size(); ++index)
    {
        const char value = spelling[index];
        if (value == '_')
        {
            if (previousSeparator)
            {
                error = "integer literal separators must appear between digits";
                return std::nullopt;
            }
            previousSeparator = true;
            continue;
        }
        previousSeparator = false;
        digits.push_back(value);
    }

    std::uint64_t magnitude = 0;
    const auto parsed = std::from_chars(
        digits.data(), digits.data() + digits.size(), magnitude,
        static_cast<int>(base));
    if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size())
    {
        error = "invalid digit or magnitude overflow in integer literal";
        return std::nullopt;
    }
    return UntypedIntegerLiteral{magnitude, base, suffix};
}

std::optional<ResolvedIntegerLiteral> resolveIntegerLiteral(
    const UntypedIntegerLiteral& literal,
    bool negative,
    std::optional<IntegerType> contextualType,
    std::string& error) noexcept
{
    error.clear();
    const auto suffixType = [&]() -> std::optional<IntegerType>
    {
        switch (literal.suffix)
        {
            case IntegerLiteralSuffix::None: return std::nullopt;
            case IntegerLiteralSuffix::I8: return IntegerType{8, true};
            case IntegerLiteralSuffix::I16: return IntegerType{16, true};
            case IntegerLiteralSuffix::I32: return IntegerType{32, true};
            case IntegerLiteralSuffix::I64:
            case IntegerLiteralSuffix::ISize: return IntegerType{64, true};
            case IntegerLiteralSuffix::U8: return IntegerType{8, false};
            case IntegerLiteralSuffix::U16: return IntegerType{16, false};
            case IntegerLiteralSuffix::U32: return IntegerType{32, false};
            case IntegerLiteralSuffix::U64:
            case IntegerLiteralSuffix::USize: return IntegerType{64, false};
        }
        return std::nullopt;
    }();
    if (suffixType.has_value() && contextualType.has_value() &&
        (suffixType->bits != contextualType->bits || suffixType->isSigned != contextualType->isSigned))
    {
        error = "integer literal suffix conflicts with the contextual type";
        return std::nullopt;
    }

    IntegerType selected;
    bool usedContext = false;
    if (suffixType.has_value()) selected = *suffixType;
    else if (contextualType.has_value()) { selected = *contextualType; usedContext = true; }
    else if (literalFits(literal.magnitude, negative, {32, true})) selected = {32, true};
    else if (literalFits(literal.magnitude, negative, {64, true})) selected = {64, true};
    else selected = {64, false};

    if (!literalFits(literal.magnitude, negative, selected))
    {
        error = "integer literal overflows its resolved type before conversion";
        return std::nullopt;
    }
    return ResolvedIntegerLiteral{literal.magnitude, negative, selected, usedContext};
}

std::optional<std::int64_t> addInteger(
    std::int64_t left,
    std::int64_t right,
    IntegerType type,
    OverflowPolicy policy) noexcept
{
    if (!type.isSigned || type.bits == 0 || type.bits > 64) return std::nullopt;
    const std::int64_t minimum = signedMinimum(type.bits);
    const std::int64_t maximum = signedMaximum(type.bits);
    const bool overflow = (right > 0 && left > maximum - right) ||
        (right < 0 && left < minimum - right);
    if (!overflow) return left + right;
    if (policy == OverflowPolicy::Checked) return std::nullopt;
    if (policy == OverflowPolicy::Saturating) return right > 0 ? maximum : minimum;
    const std::uint64_t mask = unsignedMaximum(type.bits);
    const std::uint64_t raw = (static_cast<std::uint64_t>(left) + static_cast<std::uint64_t>(right)) & mask;
    if (type.bits == 64) return static_cast<std::int64_t>(raw);
    const std::uint64_t sign = std::uint64_t{1} << (type.bits - 1U);
    return static_cast<std::int64_t>((raw ^ sign) - sign);
}

bool implicitNumericConversionIsLossless(NumericType source, NumericType target) noexcept
{
    if (source.kind == NumericKind::Floating || target.kind == NumericKind::Floating)
        return source.kind == target.kind && target.bits >= source.bits;
    if (source.isSigned == target.isSigned) return target.bits >= source.bits;
    if (!source.isSigned && target.isSigned) return target.bits > source.bits;
    return false;
}

FloatingPointContract floatingPointContract(FloatingPointMode mode) noexcept
{
    // Windows x64 strict mode fixes nearest-even, preserves denormals, forbids contraction and
    // reassociation, and therefore shares the compiler's IEEE constant-evaluation result.
    if (mode == FloatingPointMode::Strict)
        return {true, true, false, false, true, true, true};
    // Precise retains observable NaN/zero/payload/denormal behavior but explicitly allows FMA;
    // a contracted runtime result may therefore differ in the final bit from an uncontracted
    // constant expression even though reassociation remains forbidden.
    if (mode == FloatingPointMode::Precise)
        return {true, true, true, false, true, true, false};
    return {false, false, true, true, false, true, false};
}

bool strongNumericHasRuntimeWrapper() noexcept { return false; }

bool strongNumericImplicitlyConverts(
    const StrongNumericType& source,
    const StrongNumericType& target) noexcept
{
    return source.identity == target.identity;
}

bool integerSemanticsSelfCheck() noexcept
{
    constexpr IntegerType type{8, true};
    for (int left = -128; left <= 127; ++left)
    {
        for (int right = -128; right <= 127; ++right)
        {
            const int wide = left + right;
            const bool overflow = wide < -128 || wide > 127;
            const int wrapped = ((wide + 128) & 0xff) - 128;
            const int saturated = wide < -128 ? -128 : (wide > 127 ? 127 : wide);
            const auto checked = addInteger(left, right, type, OverflowPolicy::Checked);
            const auto wrapping = addInteger(left, right, type, OverflowPolicy::Wrapping);
            const auto unchecked = addInteger(left, right, type, OverflowPolicy::Unchecked);
            const auto saturating = addInteger(left, right, type, OverflowPolicy::Saturating);
            if (checked.has_value() == overflow ||
                (checked.has_value() && *checked != wide) ||
                wrapping != wrapped || unchecked != wrapped || saturating != saturated)
                return false;
        }
    }
    return true;
}
}
