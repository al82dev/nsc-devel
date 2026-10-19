#include "nativesharp/Language/NumericSemantics.h"

#include <algorithm>
#include <charconv>
#include <limits>
#include <array>

namespace nativesharp::language
{
static_assert(sizeof(Decimal128Bits) == 16);
static_assert(alignof(Decimal128Bits) == 8);

namespace
{
using DecimalCoefficient = unsigned __int128;

DecimalCoefficient decimalCoefficient(Decimal128Bits value) noexcept
{
    const DecimalCoefficient upper = decimalCoefficientHigh(value);
    return (upper << 64U) | value.low;
}

Decimal128Bits replaceDecimalCoefficient(
    Decimal128Bits value,
    DecimalCoefficient coefficient) noexcept
{
    value.low = static_cast<std::uint64_t>(coefficient);
    const std::uint64_t coefficientHigh = static_cast<std::uint64_t>(coefficient >> 64U);
    value.high &= ~DecimalCoefficientHighMask;
    value.high |= coefficientHigh & DecimalCoefficientHighMask;
    return value;
}

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

std::optional<Decimal128Bits> makeDecimal128Bits(
    std::uint64_t coefficientLow,
    std::uint32_t coefficientHigh,
    unsigned scale,
    bool negative) noexcept
{
    if (scale > DecimalMaximumScale)
    {
        return std::nullopt;
    }

    Decimal128Bits result;
    result.low = coefficientLow;
    result.high = static_cast<std::uint64_t>(coefficientHigh);
    result.high |= static_cast<std::uint64_t>(scale) << 32U;
    if (negative)
    {
        result.high |= DecimalSignMask;
    }
    return result;
}

bool decimal128BitsAreValid(Decimal128Bits value) noexcept
{
    return (value.high & DecimalReservedMask) == 0 &&
        decimalScale(value) <= DecimalMaximumScale;
}

std::uint32_t decimalCoefficientHigh(Decimal128Bits value) noexcept
{
    return static_cast<std::uint32_t>(value.high & DecimalCoefficientHighMask);
}

unsigned decimalScale(Decimal128Bits value) noexcept
{
    return static_cast<unsigned>((value.high & DecimalScaleMask) >> 32U);
}

bool decimalIsNegative(Decimal128Bits value) noexcept
{
    return (value.high & DecimalSignMask) != 0;
}

std::optional<Decimal128Bits> normalizeDecimal128Bits(Decimal128Bits value) noexcept
{
    if (!decimal128BitsAreValid(value))
    {
        return std::nullopt;
    }

    DecimalCoefficient coefficient = decimalCoefficient(value);
    unsigned scale = decimalScale(value);
    if (coefficient == 0)
    {
        return Decimal128Bits{};
    }

    while (scale != 0 && coefficient % 10U == 0)
    {
        coefficient /= 10U;
        --scale;
    }

    value = replaceDecimalCoefficient(value, coefficient);
    value.high &= ~DecimalScaleMask;
    value.high |= static_cast<std::uint64_t>(scale) << 32U;
    return value;
}

bool decimal128ValuesEqual(Decimal128Bits left, Decimal128Bits right) noexcept
{
    const auto normalizedLeft = normalizeDecimal128Bits(left);
    const auto normalizedRight = normalizeDecimal128Bits(right);
    return normalizedLeft.has_value() && normalizedRight.has_value() &&
        normalizedLeft->low == normalizedRight->low &&
        normalizedLeft->high == normalizedRight->high;
}

std::optional<std::uint64_t> decimal128StableHash(Decimal128Bits value) noexcept
{
    const auto normalized = normalizeDecimal128Bits(value);
    if (!normalized.has_value())
    {
        return std::nullopt;
    }

    constexpr std::uint64_t offset = 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    std::uint64_t hash = offset;
    for (unsigned index = 0; index < 8; ++index)
    {
        hash = (hash ^ ((normalized->low >> (index * 8U)) & 0xFFU)) * prime;
        hash = (hash ^ ((normalized->high >> (index * 8U)) & 0xFFU)) * prime;
    }
    return hash;
}

namespace
{
bool scanDecimalCoefficientText(
    std::string_view spelling,
    std::size_t& cursor,
    std::string& digits,
    int& fractionalDigits,
    std::string& error)
{
    bool sawDigit = false;
    bool sawDot = false;
    for (; cursor < spelling.size(); ++cursor)
    {
        const char value = spelling[cursor];
        if (value == '_') continue;
        if (value == '.')
        {
            if (sawDot)
            {
                error = "decimal literal has more than one decimal point";
                return false;
            }
            sawDot = true;
            continue;
        }
        if (value == 'e' || value == 'E' || value == 'm' || value == 'M') break;
        if (value < '0' || value > '9')
        {
            error = "decimal literal has an invalid digit";
            return false;
        }
        digits.push_back(value);
        sawDigit = true;
        if (sawDot) ++fractionalDigits;
    }
    if (sawDigit) return true;
    error = "decimal literal requires at least one digit";
    return false;
}

bool scanDecimalExponent(
    std::string_view spelling,
    std::size_t& cursor,
    int& exponent,
    std::string& error)
{
    if (cursor >= spelling.size() || (spelling[cursor] != 'e' && spelling[cursor] != 'E'))
        return true;
    ++cursor;
    bool exponentNegative = false;
    if (cursor < spelling.size() && (spelling[cursor] == '-' || spelling[cursor] == '+'))
    {
        exponentNegative = spelling[cursor] == '-';
        ++cursor;
    }
    if (cursor >= spelling.size())
    {
        error = "decimal exponent requires digits";
        return false;
    }
    for (; cursor < spelling.size(); ++cursor)
    {
        const char value = spelling[cursor];
        if (value == '_') continue;
        if (value < '0' || value > '9')
        {
            error = "decimal exponent has an invalid digit";
            return false;
        }
        if (exponent > 1000)
        {
            error = "decimal exponent is out of range";
            return false;
        }
        exponent = exponent * 10 + (value - '0');
    }
    if (exponentNegative) exponent = -exponent;
    return true;
}

std::optional<DecimalCoefficient> parseDecimalCoefficient(
    std::string& digits,
    std::string& error)
{
    const std::size_t first = digits.find_first_not_of('0');
    if (first == std::string::npos) return DecimalCoefficient{0};
    digits = digits.substr(first);
    DecimalCoefficient coefficient = 0;
    for (const char value : digits)
    {
        coefficient = coefficient * 10U + static_cast<unsigned>(value - '0');
        if (coefficient > ((DecimalCoefficient{1} << 96U) - 1U))
        {
            error = "decimal coefficient exceeds 96 bits";
            return std::nullopt;
        }
    }
    return coefficient;
}

bool applyDecimalLiteralScale(
    DecimalCoefficient& coefficient,
    int& scale,
    std::string& error)
{
    if (scale < 0)
    {
        for (int index = 0; index < -scale; ++index)
        {
            if (coefficient > ((DecimalCoefficient{1} << 96U) - 1U) / 10U)
            {
                error = "decimal coefficient exceeds 96 bits";
                return false;
            }
            coefficient *= 10U;
        }
        scale = 0;
    }
    if (scale <= static_cast<int>(DecimalMaximumScale)) return true;
    error = "decimal scale exceeds 28";
    return false;
}
}

std::optional<DecimalLiteralValue> parseDecimalLiteral(
    std::string_view spelling,
    std::string& error) noexcept
{
    error.clear();
    if (spelling.empty())
    {
        error = "decimal literal must not be empty";
        return std::nullopt;
    }
    std::size_t cursor = 0;
    bool negative = false;
    if (spelling[cursor] == '-' || spelling[cursor] == '+')
    {
        negative = spelling[cursor] == '-';
        ++cursor;
    }
    std::string digits;
    int fractionalDigits = 0;
    if (!scanDecimalCoefficientText(spelling, cursor, digits, fractionalDigits, error))
        return std::nullopt;
    int exponent = 0;
    if (!scanDecimalExponent(spelling, cursor, exponent, error)) return std::nullopt;
    if (cursor < spelling.size() && (spelling[cursor] == 'm' || spelling[cursor] == 'M')) ++cursor;
    if (cursor != spelling.size())
    { error = "decimal literal has trailing characters"; return std::nullopt; }

    const auto parsedCoefficient = parseDecimalCoefficient(digits, error);
    if (!parsedCoefficient.has_value()) return std::nullopt;
    if (*parsedCoefficient == 0) return DecimalLiteralValue{0, 0, 0, false};
    DecimalCoefficient coefficient = *parsedCoefficient;
    int scale = fractionalDigits - exponent;
    if (!applyDecimalLiteralScale(coefficient, scale, error)) return std::nullopt;

    DecimalLiteralValue result;
    result.coefficientLow = static_cast<std::uint64_t>(coefficient);
    result.coefficientHigh = static_cast<std::uint32_t>(coefficient >> 64U);
    result.scale = static_cast<unsigned>(scale);
    result.negative = negative;
    return result;
}

std::optional<Decimal128Bits> evaluateDecimalArithmetic(
    const Decimal128Bits& left,
    const Decimal128Bits& right,
    DecimalArithmeticOperation operation,
    std::string& error) noexcept
{
    error.clear();
    const DecimalCoefficient leftCoefficient = decimalCoefficient(left);
    const DecimalCoefficient rightCoefficient = decimalCoefficient(right);
    const unsigned leftScale = decimalScale(left);
    const unsigned rightScale = decimalScale(right);
    const bool leftNegative = decimalIsNegative(left);
    const bool rightNegative = decimalIsNegative(right);

    const auto pow10 = [](unsigned exponent) noexcept
    {
        DecimalCoefficient result = 1;
        for (unsigned index = 0; index < exponent; ++index) result *= 10U;
        return result;
    };

    DecimalCoefficient resultCoefficient = 0;
    unsigned resultScale = 0;
    bool resultNegative = false;
    if (operation == DecimalArithmeticOperation::Add ||
        operation == DecimalArithmeticOperation::Subtract ||
        operation == DecimalArithmeticOperation::Remainder)
    {
        const unsigned maxScale = std::max(leftScale, rightScale);
        const DecimalCoefficient leftAligned = leftCoefficient * pow10(maxScale - leftScale);
        const DecimalCoefficient rightAligned = rightCoefficient * pow10(maxScale - rightScale);
        if (operation == DecimalArithmeticOperation::Add)
            resultCoefficient = leftAligned + rightAligned;
        else if (operation == DecimalArithmeticOperation::Subtract)
            resultCoefficient = leftAligned - rightAligned;
        else
            // 64-bit lane: __umodti3 is not linked, matching the LLVM lowering.
            resultCoefficient = static_cast<std::uint64_t>(leftAligned) %
                static_cast<std::uint64_t>(rightAligned);
        resultScale = maxScale;
        resultNegative = leftNegative;
    }
    else if (operation == DecimalArithmeticOperation::Multiply)
    {
        resultCoefficient = leftCoefficient * rightCoefficient;
        resultScale = leftScale + rightScale;
        resultNegative = leftNegative != rightNegative;
    }
    else
    {
        const unsigned targetScale =
            std::max(leftScale, rightScale) + (leftScale > rightScale ? 1U : 0U);
        const unsigned exponent = targetScale + rightScale - leftScale;
        const std::uint64_t leftCoeff64 = static_cast<std::uint64_t>(leftCoefficient);
        const std::uint64_t rightCoeff64 = static_cast<std::uint64_t>(rightCoefficient);
        std::uint64_t numerator = leftCoeff64;
        for (unsigned index = 0; index < exponent; ++index) numerator *= 10U;
        // 64-bit lane (__udivti3 not linked) + round half-up.
        resultCoefficient = (numerator + rightCoeff64 / 2U) / rightCoeff64;
        resultScale = targetScale;
        resultNegative = leftNegative != rightNegative;
    }

    if (resultScale > DecimalMaximumScale)
    { error = "decimal scale exceeds 28"; return std::nullopt; }
    if (resultCoefficient > ((DecimalCoefficient{1} << 96U) - 1U))
    { error = "decimal coefficient exceeds 96 bits"; return std::nullopt; }
    return makeDecimal128Bits(
        static_cast<std::uint64_t>(resultCoefficient),
        static_cast<std::uint32_t>(resultCoefficient >> 64U),
        resultScale,
        resultNegative);
}

std::string decimalToStringText(const Decimal128Bits& value) noexcept
{
    const DecimalCoefficient coefficient = decimalCoefficient(value);
    const unsigned scale = decimalScale(value);
    const bool negative = decimalIsNegative(value);

    std::string digits;
    if (coefficient == 0)
    {
        digits = "0";
    }
    else
    {
        DecimalCoefficient remaining = coefficient;
        while (remaining > 0)
        {
            digits.push_back(static_cast<char>('0' + static_cast<int>(remaining % 10U)));
            remaining /= 10U;
        }
        std::reverse(digits.begin(), digits.end());
    }

    std::string result;
    if (negative) result.push_back('-');
    if (scale == 0)
    {
        result += digits;
    }
    else if (scale >= digits.size())
    {
        result += "0.";
        result.append(scale - digits.size(), '0');
        result += digits;
    }
    else
    {
        result += digits.substr(0, digits.size() - scale);
        result.push_back('.');
        result += digits.substr(digits.size() - scale);
    }
    return result;
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

namespace
{
__int128 integerOperandValue(
    std::uint64_t bits,
    IntegerType type,
    std::uint64_t mask) noexcept
{
    bits &= mask;
    if (!type.isSigned) return static_cast<__int128>(bits);
    const std::uint64_t sign = std::uint64_t{1} << (type.bits - 1U);
    if ((bits & sign) == 0) return static_cast<__int128>(bits);
    return static_cast<__int128>(bits) - (static_cast<__int128>(1) << type.bits);
}

bool evaluateRequestedIntegerOperation(
    const IntegerArithmeticRequest& request,
    __int128 left,
    __int128 right,
    __int128 minimum,
    __int128 maximum,
    std::uint64_t mask,
    __int128& wide,
    bool& forcedOverflow,
    std::string& error)
{
    switch (request.operation)
    {
        case IntegerArithmeticOperation::Add: wide = left + right; break;
        case IntegerArithmeticOperation::Subtract: wide = left - right; break;
        case IntegerArithmeticOperation::Multiply: wide = left * right; break;
        case IntegerArithmeticOperation::Divide:
            if (right == 0)
            {
                error = "integer division by zero";
                return false;
            }
            if (request.type.isSigned && left == minimum && right == -1)
            {
                wide = maximum + 1;
                forcedOverflow = true;
            }
            else if (request.type.isSigned)
                wide = static_cast<std::int64_t>(left) / static_cast<std::int64_t>(right);
            else
                wide = static_cast<std::uint64_t>(left) / static_cast<std::uint64_t>(right);
            break;
        case IntegerArithmeticOperation::ShiftLeft:
        case IntegerArithmeticOperation::ShiftRight:
        {
            std::uint64_t shift = request.rightBits;
            if (shift >= request.type.bits)
            {
                forcedOverflow = true;
                if (request.policy == OverflowPolicy::Checked)
                {
                    error = "integer shift count is outside the operand width";
                    return false;
                }
                shift %= request.type.bits;
            }
            if (request.operation == IntegerArithmeticOperation::ShiftLeft)
                wide = left * (static_cast<__int128>(1) << shift);
            else
                wide = request.type.isSigned
                    ? left >> shift
                    : static_cast<__int128>((request.leftBits & mask) >> shift);
            break;
        }
    }
    return true;
}
}

std::optional<IntegerArithmeticResult> evaluateIntegerArithmetic(
    const IntegerArithmeticRequest& request,
    std::string& error) noexcept
{
    error.clear();
    if (request.type.bits == 0 || request.type.bits > 64)
    {
        error = "integer arithmetic width must be between 1 and 64 bits";
        return std::nullopt;
    }

    const std::uint64_t mask = unsignedMaximum(request.type.bits);
    const __int128 left = integerOperandValue(request.leftBits, request.type, mask);
    const __int128 right = integerOperandValue(request.rightBits, request.type, mask);
    const __int128 minimum = request.type.isSigned
        ? -(static_cast<__int128>(1) << (request.type.bits - 1U))
        : 0;
    const __int128 maximum = request.type.isSigned
        ? (static_cast<__int128>(1) << (request.type.bits - 1U)) - 1
        : request.type.bits == 64
            ? static_cast<__int128>(std::numeric_limits<std::uint64_t>::max())
            : (static_cast<__int128>(1) << request.type.bits) - 1;

    __int128 wide = 0;
    bool forcedOverflow = false;
    if (!evaluateRequestedIntegerOperation(
            request, left, right, minimum, maximum, mask, wide, forcedOverflow, error))
        return std::nullopt;

    const bool overflow = forcedOverflow || wide < minimum || wide > maximum;
    if (overflow && request.policy == OverflowPolicy::Checked)
    {
        error = "checked integer operation overflowed";
        return std::nullopt;
    }

    if (overflow && request.policy == OverflowPolicy::Saturating)
        wide = wide < minimum ? minimum : maximum;
    const std::uint64_t bits = static_cast<std::uint64_t>(wide) & mask;
    return IntegerArithmeticResult{
        bits,
        overflow,
        overflow && request.policy == OverflowPolicy::Unchecked};
}

std::int64_t integerResultAsSigned(
    const IntegerArithmeticResult& result,
    IntegerType type) noexcept
{
    if (!type.isSigned || type.bits == 0 || type.bits > 64)
        return static_cast<std::int64_t>(result.bits);
    if (type.bits == 64) return static_cast<std::int64_t>(result.bits);
    const std::uint64_t mask = unsignedMaximum(type.bits);
    const std::uint64_t sign = std::uint64_t{1} << (type.bits - 1U);
    const std::uint64_t bits = result.bits & mask;
    return static_cast<std::int64_t>((bits ^ sign) - sign);
}

bool implicitNumericConversionIsLossless(NumericType source, NumericType target) noexcept
{
    if (source.kind == NumericKind::Floating || target.kind == NumericKind::Floating)
        return source.kind == target.kind && target.bits >= source.bits;
    if (source.isSigned == target.isSigned) return target.bits >= source.bits;
    if (!source.isSigned && target.isSigned) return target.bits > source.bits;
    return false;
}

NumericConversionDecision classifyNumericConversion(
    NumericType source,
    NumericType target,
    std::optional<NumericConstant> constant,
    NumericConversionUse use) noexcept
{
    if (source.bits == 0 || target.bits == 0 ||
        source.bits > 64 || target.bits > 64)
        return {};

    if (source.kind == target.kind && source.bits == target.bits &&
        source.isSigned == target.isSigned && use != NumericConversionUse::EnumUnderlying)
        return {NumericConversionKind::Identity, 0, true, true, false};

    const bool sourceInteger = source.kind == NumericKind::Integer || source.kind == NumericKind::Enum;
    const bool targetInteger = target.kind == NumericKind::Integer || target.kind == NumericKind::Enum;
    if (use == NumericConversionUse::EnumUnderlying ||
        source.kind == NumericKind::Enum || target.kind == NumericKind::Enum)
    {
        if (!sourceInteger || !targetInteger) return {};
        return {NumericConversionKind::ExplicitEnumUnderlying, 4, false, false, true};
    }

    if (constant.has_value() && !constant->floating)
    {
        if (target.kind == NumericKind::Integer && literalFits(
                constant->magnitude, constant->negative,
                IntegerType{target.bits, target.isSigned}))
            return {NumericConversionKind::ImplicitConstant, 1, true, true, false};
        if (target.kind == NumericKind::Floating)
        {
            const unsigned precision = target.bits <= 32 ? 24U : 53U;
            const std::uint64_t exactLimit = std::uint64_t{1} << precision;
            if (constant->magnitude <= exactLimit)
                return {NumericConversionKind::ImplicitConstant, 1, true, true, false};
        }
    }

    if (sourceInteger && targetInteger)
    {
        if (implicitNumericConversionIsLossless(source, target))
            return {NumericConversionKind::ImplicitLossless, 1, true, true, false};
        const bool signChange = source.isSigned != target.isSigned;
        return {signChange ? NumericConversionKind::ExplicitSignChange
                           : NumericConversionKind::ExplicitNarrowing,
            3, false, false, true};
    }

    if (source.kind == NumericKind::Floating && target.kind == NumericKind::Floating)
    {
        if (target.bits >= source.bits)
            return {NumericConversionKind::ImplicitLossless, 1, true, true, false};
        return {NumericConversionKind::ExplicitNarrowing, 3, false, false, false};
    }

    if (sourceInteger && target.kind == NumericKind::Floating)
    {
        const unsigned precision = target.bits <= 32 ? 24U : 53U;
        const unsigned required = source.isSigned ? source.bits - 1U : source.bits;
        if (required <= precision)
            return {NumericConversionKind::ImplicitLossless, 1, true, true, false};
        return {NumericConversionKind::ExplicitFloatInteger, 3, false, false, false};
    }
    if (source.kind == NumericKind::Floating && targetInteger)
        return {NumericConversionKind::ExplicitFloatInteger, 3, false, false, true};
    return {};
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

std::optional<StrongNumericPlan> buildStrongNumericPlan(
    const StrongNumericDefinition& definition,
    std::string& error) noexcept
{
    error.clear();
    if (definition.identity.empty())
    {
        error = "strong numeric newtype requires a nominal identity";
        return std::nullopt;
    }
    if (definition.fieldCount != 1)
    {
        error = "strong numeric newtype must contain exactly one representation field";
        return std::nullopt;
    }
    if (definition.representation.bits == 0 || definition.representation.bits > 64 ||
        definition.representation.bits % 8 != 0)
    {
        error = "strong numeric representation requires a fundamental byte-sized numeric type";
        return std::nullopt;
    }
    if (definition.ffiExported && !definition.reprTransparent)
    {
        error = "FFI-exported strong numeric newtype requires repr(transparent)";
        return std::nullopt;
    }
    if (definition.derivesCopy && !definition.derivesMove)
    {
        error = "a Copy strong numeric newtype must also satisfy Move";
        return std::nullopt;
    }

    StrongNumericPlan result;
    result.type = StrongNumericType{definition.identity, definition.representation};
    result.size = definition.representation.bits / 8;
    result.alignment = result.size > 8 ? 8 : result.size;
    result.constGenericArity = definition.constGenericArity;
    result.abiTransparent = definition.reprTransparent;
    result.derivesCopy = definition.derivesCopy;
    result.derivesMove = definition.derivesMove;
    result.operators = definition.operators;
    return result;
}

bool strongNumericOperatorAllowed(
    const StrongNumericPlan& left,
    const StrongNumericPlan& right,
    StrongNumericOperator operation) noexcept
{
    if (left.type.identity != right.type.identity ||
        left.constGenericArity != right.constGenericArity)
        return false;
    return std::ranges::find(left.operators, operation) != left.operators.end();
}

std::optional<ConstantPointer> ConstantMemory::allocate(
    std::size_t size,
    std::size_t alignment,
    std::string& error)
{
    error.clear();
    if (size == 0 || alignment == 0 || (alignment & (alignment - 1U)) != 0)
    {
        error = "constant allocation requires nonzero size and power-of-two alignment";
        return std::nullopt;
    }
    const std::uint64_t identity = nextIdentity_++;
    allocations_.push_back(ConstantAllocation{identity, size, alignment, true});
    return ConstantPointer{identity, 0};
}

std::optional<ConstantPointer> ConstantMemory::offset(
    ConstantPointer pointer,
    std::ptrdiff_t delta,
    std::string& error) const
{
    error.clear();
    const auto allocation = std::ranges::find(
        allocations_, pointer.allocation, &ConstantAllocation::identity);
    if (allocation == allocations_.end() || !allocation->live)
    {
        error = "constant pointer has no live provenance";
        return std::nullopt;
    }
    if (pointer.offset > allocation->size)
    {
        error = "constant pointer offset is outside its provenance allocation";
        return std::nullopt;
    }
    const auto current = static_cast<std::ptrdiff_t>(pointer.offset);
    if ((delta < 0 && current < -delta) ||
        (delta > 0 && static_cast<std::size_t>(delta) > allocation->size - pointer.offset))
    {
        error = "constant pointer arithmetic escaped its allocation";
        return std::nullopt;
    }
    return ConstantPointer{
        pointer.allocation,
        static_cast<std::size_t>(current + delta)};
}

bool ConstantMemory::validateDereference(
    ConstantPointer pointer,
    std::size_t width,
    std::string& error) const
{
    error.clear();
    const auto allocation = std::ranges::find(
        allocations_, pointer.allocation, &ConstantAllocation::identity);
    if (allocation == allocations_.end() || !allocation->live)
    {
        error = "constant dereference has no live provenance";
        return false;
    }
    if (width == 0 || pointer.offset > allocation->size ||
        width > allocation->size - pointer.offset)
    {
        error = "constant dereference is outside its allocation";
        return false;
    }
    return true;
}

bool ConstantMemory::deallocate(std::uint64_t identity, std::string& error)
{
    error.clear();
    const auto allocation = std::ranges::find(
        allocations_, identity, &ConstantAllocation::identity);
    if (allocation == allocations_.end() || !allocation->live)
    {
        error = "constant allocation is unknown or already dead";
        return false;
    }
    allocation->live = false;
    return true;
}

bool activateConstantUnionMember(
    ConstantUnionState& state,
    std::string_view member,
    std::size_t memberSize,
    std::string& error)
{
    error.clear();
    if (member.empty() || memberSize == 0 || memberSize > state.storageSize)
    {
        error = "constant union member does not fit its storage";
        return false;
    }
    state.activeMember = std::string(member);
    return true;
}

bool readConstantUnionMember(
    const ConstantUnionState& state,
    std::string_view member,
    std::string& error) noexcept
{
    error.clear();
    if (state.activeMember != member)
    {
        error = "constant union read targets an inactive member";
        return false;
    }
    return true;
}

std::optional<IntegerArithmeticResult> evaluateConstantIntegerArithmetic(
    IntegerArithmeticRequest request,
    std::string& error) noexcept
{
    request.domain = NumericEvaluationDomain::Constant;
    auto result = evaluateIntegerArithmetic(request, error);
    if (result.has_value() && result->undefinedBehavior)
    {
        error = "constant evaluation encountered undefined integer overflow";
        return std::nullopt;
    }
    return result;
}

std::optional<IntegerArithmeticResult> executeConstantIntegerLoop(
    std::uint64_t seedBits,
    std::uint64_t operandBits,
    std::size_t iterations,
    std::size_t iterationBudget,
    IntegerArithmeticOperation operation,
    IntegerType type,
    OverflowPolicy policy,
    std::string& error) noexcept
{
    error.clear();
    if (iterations > iterationBudget)
    {
        error = "constant loop exceeded its deterministic iteration budget";
        return std::nullopt;
    }
    IntegerArithmeticResult current{seedBits, false, false};
    for (std::size_t iteration = 0; iteration < iterations; ++iteration)
    {
        const auto next = evaluateConstantIntegerArithmetic(
            IntegerArithmeticRequest{operation, current.bits, operandBits, type,
                policy, NumericEvaluationDomain::Constant}, error);
        if (!next.has_value()) return std::nullopt;
        current = *next;
    }
    return current;
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
