#include "nativesharp/Language/NumericSemantics.h"

#include "../support/TestContext.h"

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;
    const IntegerType i8{8, true};

    IntegerArithmeticRequest request{
        IntegerArithmeticOperation::Add, 127, 1, i8,
        OverflowPolicy::Checked, NumericEvaluationDomain::Runtime};
    tests.expect(!evaluateIntegerArithmetic(request, error).has_value() &&
            error.find("overflowed") != std::string::npos,
        "checked addition reports overflow causally");
    request.policy = OverflowPolicy::Wrapping;
    const auto wrapped = evaluateIntegerArithmetic(request, error);
    tests.expect(wrapped.has_value() && wrapped->overflowed &&
            integerResultAsSigned(*wrapped, i8) == -128 && !wrapped->undefinedBehavior,
        "wrapping addition is explicit and defined");
    request.policy = OverflowPolicy::Saturating;
    const auto saturated = evaluateIntegerArithmetic(request, error);
    tests.expect(saturated.has_value() && integerResultAsSigned(*saturated, i8) == 127,
        "saturating addition clamps to the signed maximum");
    request.policy = OverflowPolicy::Unchecked;
    const auto unchecked = evaluateIntegerArithmetic(request, error);
    tests.expect(unchecked.has_value() && unchecked->undefinedBehavior,
        "unchecked overflow is marked as UB instead of silently inheriting build mode behavior");

    request = {IntegerArithmeticOperation::Subtract, static_cast<std::uint64_t>(-128), 1,
        i8, OverflowPolicy::Saturating, NumericEvaluationDomain::Constant};
    const auto sub = evaluateIntegerArithmetic(request, error);
    request = {IntegerArithmeticOperation::Multiply, 64, 4,
        i8, OverflowPolicy::Wrapping, NumericEvaluationDomain::Intrinsic};
    const auto mul = evaluateIntegerArithmetic(request, error);
    tests.expect(sub.has_value() && integerResultAsSigned(*sub, i8) == -128 &&
            mul.has_value() && integerResultAsSigned(*mul, i8) == 0,
        "subtraction and multiplication obey the selected policy in consteval and intrinsics");

    request = {IntegerArithmeticOperation::Divide, static_cast<std::uint64_t>(-128),
        static_cast<std::uint64_t>(-1), i8, OverflowPolicy::Checked,
        NumericEvaluationDomain::Runtime};
    tests.expect(!evaluateIntegerArithmetic(request, error).has_value(),
        "signed minimum divided by minus one is checked overflow");
    request.rightBits = 0;
    tests.expect(!evaluateIntegerArithmetic(request, error).has_value() &&
            error.find("division by zero") != std::string::npos,
        "division by zero is rejected under every overflow policy");

    request = {IntegerArithmeticOperation::ShiftLeft, 1, 8, i8,
        OverflowPolicy::Checked, NumericEvaluationDomain::Runtime};
    tests.expect(!evaluateIntegerArithmetic(request, error).has_value() &&
            error.find("shift count") != std::string::npos,
        "checked shifts reject counts outside the operand width");

    request = {IntegerArithmeticOperation::Add, 255, 1, {8, false},
        OverflowPolicy::Wrapping, NumericEvaluationDomain::SimdLane};
    const auto unsignedWrap = evaluateIntegerArithmetic(request, error);
    tests.expect(unsignedWrap.has_value() && unsignedWrap->bits == 0,
        "unsigned and SIMD-lane arithmetic use the same explicit policy");

    request = {IntegerArithmeticOperation::Add, 100, 20, i8,
        OverflowPolicy::Checked, NumericEvaluationDomain::Runtime};
    const auto runtime = evaluateIntegerArithmetic(request, error);
    request.domain = NumericEvaluationDomain::Constant;
    const auto constant = evaluateIntegerArithmetic(request, error);
    request.domain = NumericEvaluationDomain::Intrinsic;
    const auto intrinsic = evaluateIntegerArithmetic(request, error);
    tests.expect(runtime.has_value() && constant.has_value() && intrinsic.has_value() &&
            runtime->bits == constant->bits && runtime->bits == intrinsic->bits,
        "runtime, comptime, optimization, SIMD, and intrinsic paths share one arithmetic contract");
    return tests.result();
}
