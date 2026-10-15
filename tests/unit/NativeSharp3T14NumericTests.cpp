#include "nativesharp/Language/NumericSemantics.h"

#include "../support/TestContext.h"

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;

    const auto maximumDecimal = makeDecimal128Bits(
        0xFFFFFFFFFFFFFFFFull,
        0xFFFFFFFFU,
        DecimalMaximumScale,
        true);
    tests.expect(sizeof(Decimal128Bits) == 16 && alignof(Decimal128Bits) == 8 &&
            maximumDecimal.has_value() && decimal128BitsAreValid(*maximumDecimal) &&
            decimalCoefficientHigh(*maximumDecimal) == 0xFFFFFFFFU &&
            decimalScale(*maximumDecimal) == DecimalMaximumScale &&
            decimalIsNegative(*maximumDecimal),
        "decimal payload is 128 bits with a 96-bit coefficient, scale 0..28, and independent sign");

    Decimal128Bits reservedBits{};
    reservedBits.high = std::uint64_t{1} << 38U;
    tests.expect(!makeDecimal128Bits(0, 0, DecimalMaximumScale + 1, false).has_value() &&
            !decimal128BitsAreValid(reservedBits) &&
            !normalizeDecimal128Bits(reservedBits).has_value(),
        "decimal rejects out-of-range scales and reserved payload states instead of encoding NaN or infinity");

    const auto onePointZero = makeDecimal128Bits(10, 0, 1, false);
    const auto onePointZeroZero = makeDecimal128Bits(100, 0, 2, false);
    const auto negativeScaledZero = makeDecimal128Bits(0, 0, 17, true);
    const auto normalizedZero = negativeScaledZero.has_value() ?
        normalizeDecimal128Bits(*negativeScaledZero) : std::nullopt;
    tests.expect(onePointZero.has_value() && onePointZeroZero.has_value() &&
            negativeScaledZero.has_value() &&
            decimal128ValuesEqual(*onePointZero, *onePointZeroZero) &&
            decimal128StableHash(*onePointZero) == decimal128StableHash(*onePointZeroZero) &&
            normalizedZero.has_value() && normalizedZero->low == 0 && normalizedZero->high == 0,
        "decimal equality and hashing normalize trailing zeroes, signed zero, and scale without mutating operands");

    tests.expect(parseIntegerLiteralMagnitude("0xff_ff", error) == 65535 &&
            parseIntegerLiteralMagnitude("0b1010", error) == 10,
        "numeric literals normalize separators and bases before contextual typing");
    tests.expect(literalFits(127, false, {8, true}) && literalFits(128, true, {8, true}) &&
            !literalFits(128, false, {8, true}) && !literalFits(1, true, {8, false}),
        "literal overflow is diagnosed against signedness and width before conversion");

    tests.expect(!addInteger(127, 1, {8, true}, OverflowPolicy::Checked).has_value() &&
            addInteger(127, 1, {8, true}, OverflowPolicy::Wrapping) == -128 &&
            addInteger(127, 1, {8, true}, OverflowPolicy::Saturating) == 127,
        "checked, wrapping, and saturating arithmetic have distinct stable results");
    tests.expect(implicitNumericConversionIsLossless({NumericKind::Integer, 8, false},
            {NumericKind::Integer, 16, true}) &&
            !implicitNumericConversionIsLossless({NumericKind::Integer, 16, true},
                {NumericKind::Integer, 16, false}),
        "implicit numeric conversions are limited to provably lossless pairs");

    const auto strict = floatingPointContract(FloatingPointMode::Strict);
    const auto fast = floatingPointContract(FloatingPointMode::Fast);
    tests.expect(strict.preservesSignedZero && strict.preservesNanSemantics &&
            !strict.allowsReassociation && fast.allowsReassociation && !fast.preservesSignedZero,
        "floating-point modes state NaN, signed-zero, contraction, and reassociation guarantees");
    const StrongNumericType meters{"Demo.Meters", {NumericKind::Integer, 32, true}};
    const StrongNumericType seconds{"Demo.Seconds", {NumericKind::Integer, 32, true}};
    tests.expect(!strongNumericHasRuntimeWrapper() && strongNumericImplicitlyConverts(meters, meters) &&
            !strongNumericImplicitlyConverts(meters, seconds),
        "strong numeric newtypes are zero-overhead but never dimensionally magical");
    tests.expect(integerSemanticsSelfCheck(),
        "integer policies pass an independent exhaustive int8 reference check");
    return tests.result();
}
