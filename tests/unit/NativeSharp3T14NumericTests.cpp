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

    const auto zeroPointOne = parseDecimalLiteral("0.1", error);
    tests.expect(zeroPointOne.has_value() && error.empty() &&
            zeroPointOne->coefficientLow == 1 && zeroPointOne->coefficientHigh == 0 &&
            zeroPointOne->scale == 1 && !zeroPointOne->negative,
        "0.1 parses to the exact coefficient 1 / scale 1 without binary floating point");
    const auto onePointFive = parseDecimalLiteral("1.5m", error);
    tests.expect(onePointFive.has_value() && onePointFive->coefficientLow == 15 &&
            onePointFive->scale == 1,
        "1.5m parses to coefficient 15 / scale 1 and accepts the suffix");
    const auto hundred = parseDecimalLiteral("1e2", error);
    tests.expect(hundred.has_value() && hundred->coefficientLow == 100 && hundred->scale == 0,
        "1e2 normalizes the exponent into the coefficient");
    const auto negative = parseDecimalLiteral("-0.001", error);
    tests.expect(negative.has_value() && negative->coefficientLow == 1 &&
            negative->scale == 3 && negative->negative,
        "-0.001 preserves the sign and the fractional scale");
    const auto bigScale = parseDecimalLiteral("0.00000000000000000000000000001", error);
    tests.expect(!bigScale.has_value() && error.find("scale exceeds 28") != std::string::npos,
        "a scale above 28 is rejected with a causal error");
    const auto tooBig = parseDecimalLiteral(
        "12345678901234567890123456789012345678901234567890", error);
    tests.expect(!tooBig.has_value() && error.find("coefficient exceeds 96 bits") != std::string::npos,
        "a coefficient above 96 bits is rejected with a causal error");

    const auto oneFiveBits = makeDecimal128Bits(15, 0, 1, false);
    const auto twoBits = makeDecimal128Bits(2, 0, 0, false);
    const auto threeBits = makeDecimal128Bits(3, 0, 0, false);
    const auto halfBits = makeDecimal128Bits(5, 0, 1, false);
    const auto addResult = evaluateDecimalArithmetic(
        *oneFiveBits, *twoBits, DecimalArithmeticOperation::Add, error);
    tests.expect(addResult.has_value() && error.empty() && decimalScale(*addResult) == 1 &&
            addResult->low == 35 && decimalCoefficientHigh(*addResult) == 0,
        "decimal add aligns to the max scale (1.5 + 2 = 3.5)");
    const auto divResult = evaluateDecimalArithmetic(
        *halfBits, *threeBits, DecimalArithmeticOperation::Divide, error);
    tests.expect(divResult.has_value() && decimalScale(*divResult) == 2 && divResult->low == 17,
        "decimal divide rounds half-up to the target scale (0.5 / 3 = 0.17)");
    const auto mulResult = evaluateDecimalArithmetic(
        *oneFiveBits, *twoBits, DecimalArithmeticOperation::Multiply, error);
    tests.expect(mulResult.has_value() && decimalScale(*mulResult) == 1 && mulResult->low == 30,
        "decimal multiply sums scales (1.5 * 2 = 3.0)");
    const auto tiny = makeDecimal128Bits(1, 0, 28, false);
    const auto tenth = makeDecimal128Bits(1, 0, 1, false);
    const auto overflow = evaluateDecimalArithmetic(
        *tiny, *tenth, DecimalArithmeticOperation::Multiply, error);
    tests.expect(!overflow.has_value() && error.find("scale exceeds 28") != std::string::npos,
        "decimal multiply scale overflow above 28 is rejected");
    return tests.result();
}
