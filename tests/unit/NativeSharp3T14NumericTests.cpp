#include "nativesharp/Language/NumericSemantics.h"

#include "../support/TestContext.h"

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;
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
    tests.expect(constevalMatchesRuntime(127, 1, {8, true}, OverflowPolicy::Wrapping) &&
            constevalMatchesRuntime(127, 1, {8, true}, OverflowPolicy::Checked),
        "constant and runtime integer evaluation share exactly one overflow implementation");
    return tests.result();
}
