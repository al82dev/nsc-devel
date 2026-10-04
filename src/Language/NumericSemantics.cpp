#include "nativesharp/Language/NumericSemantics.h"

#include <charconv>
#include <limits>

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
