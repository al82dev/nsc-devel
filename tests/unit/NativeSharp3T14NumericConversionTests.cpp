#include "nativesharp/Language/NumericSemantics.h"

#include "../support/TestContext.h"

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;

    const NumericType i8{NumericKind::Integer, 8, true};
    const NumericType u8{NumericKind::Integer, 8, false};
    const NumericType i16{NumericKind::Integer, 16, true};
    const NumericType u16{NumericKind::Integer, 16, false};
    const NumericType i32{NumericKind::Integer, 32, true};
    const NumericType u32{NumericKind::Integer, 32, false};
    const NumericType i64{NumericKind::Integer, 64, true};
    const NumericType f32{NumericKind::Floating, 32, true};
    const NumericType f64{NumericKind::Floating, 64, true};

    const auto identity = classifyNumericConversion(i32, i32, std::nullopt,
        NumericConversionUse::Overload);
    tests.expect(identity.kind == NumericConversionKind::Identity &&
            identity.implicit && identity.overloadRank == 0,
        "identity is the best overload conversion");

    const auto signedWiden = classifyNumericConversion(i8, i16, std::nullopt,
        NumericConversionUse::Assignment);
    const auto unsignedToSigned = classifyNumericConversion(u8, i16, std::nullopt,
        NumericConversionUse::Assignment);
    tests.expect(signedWiden.implicit && signedWiden.lossless &&
            unsignedToSigned.implicit && unsignedToSigned.lossless,
        "same-sign widening and wider unsigned-to-signed conversion are implicit");

    const auto signChange = classifyNumericConversion(i16, u16, std::nullopt,
        NumericConversionUse::Assignment);
    tests.expect(!signChange.implicit &&
            signChange.kind == NumericConversionKind::ExplicitSignChange &&
            signChange.requiresOverflowPolicy,
        "same-width sign changes require an explicit cast and overflow policy");

    const auto narrow = classifyNumericConversion(i64, i32, std::nullopt,
        NumericConversionUse::Assignment);
    tests.expect(narrow.kind == NumericConversionKind::ExplicitNarrowing &&
            narrow.requiresOverflowPolicy,
        "integer narrowing is explicit and follows the active overflow context");

    const auto exactConstant = classifyNumericConversion(i32, u8,
        NumericConstant{255, false, false}, NumericConversionUse::ConstGeneric);
    const auto overflowingConstant = classifyNumericConversion(i32, u8,
        NumericConstant{256, false, false}, NumericConversionUse::ConstGeneric);
    tests.expect(exactConstant.kind == NumericConversionKind::ImplicitConstant &&
            exactConstant.implicit && !overflowingConstant.implicit,
        "representable integer constants convert implicitly while overflowing constants do not");

    const auto floatWiden = classifyNumericConversion(f32, f64, std::nullopt,
        NumericConversionUse::Assignment);
    const auto floatNarrow = classifyNumericConversion(f64, f32, std::nullopt,
        NumericConversionUse::Assignment);
    tests.expect(floatWiden.implicit && floatWiden.lossless &&
            floatNarrow.kind == NumericConversionKind::ExplicitNarrowing,
        "floating widening is implicit and floating narrowing is explicit");

    const auto exactIntFloat = classifyNumericConversion(u16, f32, std::nullopt,
        NumericConversionUse::Assignment);
    const auto lossyIntFloat = classifyNumericConversion(u32, f32, std::nullopt,
        NumericConversionUse::Assignment);
    tests.expect(exactIntFloat.implicit && !lossyIntFloat.implicit &&
            lossyIntFloat.kind == NumericConversionKind::ExplicitFloatInteger,
        "integer-to-float conversion is implicit only when every source value is exact");

    const auto floatInt = classifyNumericConversion(f64, i32, std::nullopt,
        NumericConversionUse::Assignment);
    tests.expect(!floatInt.implicit && floatInt.requiresOverflowPolicy,
        "float-to-integer conversion is explicit and consumes the overflow policy");

    const NumericType color{NumericKind::Enum, 8, false};
    const auto enumUnderlying = classifyNumericConversion(color, u8, std::nullopt,
        NumericConversionUse::EnumUnderlying);
    tests.expect(enumUnderlying.kind == NumericConversionKind::ExplicitEnumUnderlying &&
            !enumUnderlying.implicit,
        "enum underlying conversions remain explicit even when representation matches");

    const auto scalar = classifyNumericConversion(u8, i16, std::nullopt,
        NumericConversionUse::Assignment);
    const auto splat = classifyNumericConversion(u8, i16, std::nullopt,
        NumericConversionUse::SimdSplat);
    tests.expect(scalar.kind == splat.kind && scalar.overloadRank == splat.overloadRank,
        "SIMD splats reuse the scalar conversion matrix without a hidden widening rule");
    return tests.result();
}
