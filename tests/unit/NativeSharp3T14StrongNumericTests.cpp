#include "nativesharp/Language/NumericSemantics.h"

#include "../support/TestContext.h"

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;

    StrongNumericDefinition meters;
    meters.identity = "Physics.Meters";
    meters.representation = {NumericKind::Floating, 32, true};
    meters.reprTransparent = true;
    meters.ffiExported = true;
    meters.derivesCopy = true;
    meters.constGenericArity = 1;
    meters.operators = {StrongNumericOperator::Add, StrongNumericOperator::Subtract,
        StrongNumericOperator::Compare};
    const auto meterPlan = buildStrongNumericPlan(meters, error);
    tests.expect(meterPlan.has_value() && meterPlan->size == 4 && meterPlan->alignment == 4 &&
            meterPlan->abiTransparent && meterPlan->zeroRuntimeOverhead,
        "repr(transparent) strong numerics preserve the representation ABI with zero wrapper cost");
    tests.expect(meterPlan.has_value() && meterPlan->constructionIsExplicit &&
            meterPlan->extractionIsExplicit && meterPlan->constGenericArity == 1,
        "construction, extraction, and const-generic identity stay explicit");

    const auto sameMeterPlan = buildStrongNumericPlan(meters, error);
    tests.expect(meterPlan.has_value() && sameMeterPlan.has_value() &&
            strongNumericOperatorAllowed(*meterPlan, *sameMeterPlan, StrongNumericOperator::Add) &&
            !strongNumericOperatorAllowed(*meterPlan, *sameMeterPlan, StrongNumericOperator::Multiply),
        "only declared operator traits are available between the same nominal unit");

    StrongNumericDefinition seconds = meters;
    seconds.identity = "Physics.Seconds";
    const auto secondPlan = buildStrongNumericPlan(seconds, error);
    tests.expect(meterPlan.has_value() && secondPlan.has_value() &&
            !strongNumericOperatorAllowed(*meterPlan, *secondPlan, StrongNumericOperator::Add) &&
            !strongNumericImplicitlyConverts(meterPlan->type, secondPlan->type),
        "equal representation never creates dimensional conversion magic");

    StrongNumericDefinition internal = meters;
    internal.identity = "Physics.InternalTicks";
    internal.ffiExported = false;
    internal.reprTransparent = false;
    const auto internalPlan = buildStrongNumericPlan(internal, error);
    tests.expect(internalPlan.has_value() && !internalPlan->abiTransparent &&
            internalPlan->zeroRuntimeOverhead,
        "an internal nominal newtype remains zero-cost without claiming transparent FFI ABI");

    StrongNumericDefinition invalidFfi = meters;
    invalidFfi.reprTransparent = false;
    tests.expect(!buildStrongNumericPlan(invalidFfi, error).has_value() &&
            error.find("repr(transparent)") != std::string::npos,
        "FFI export fails closed unless transparent representation is explicit");

    StrongNumericDefinition extraField = meters;
    extraField.fieldCount = 2;
    tests.expect(!buildStrongNumericPlan(extraField, error).has_value() &&
            error.find("exactly one") != std::string::npos,
        "transparent strong numerics cannot hide additional runtime fields");

    StrongNumericDefinition badCopy = meters;
    badCopy.derivesMove = false;
    tests.expect(!buildStrongNumericPlan(badCopy, error).has_value() &&
            error.find("also satisfy Move") != std::string::npos,
        "derive contracts preserve the Copy-implies-Move ownership invariant");
    return tests.result();
}
