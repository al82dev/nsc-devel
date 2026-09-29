#include "nativesharp/Language/CallableSemantics.h"

#include "../support/TestContext.h"

#include <array>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    FunctionType native{{"builtin:int"}, "builtin:int", CallingConvention::NativeSharp,
        "", false, false, false};
    FunctionType c = native;
    c.callingConvention = CallingConvention::C;
    tests.expect(functionTypeIdentity(native) != functionTypeIdentity(c) &&
            !functionTypesAssignable(native, c),
        "function identity includes calling convention and ABI-relevant effects");
    FunctionType nullable = native;
    nullable.nullable = true;
    tests.expect(functionTypesAssignable(native, nullable) && !functionTypesAssignable(nullable, native),
        "non-null function pointers widen to nullable pointers but not conversely");

    ClosureLayout layout;
    std::string error;
    const std::array<Capture, 2> captures = {{{"small", 1, 1, CaptureMode::Copy, true},
        {"resource", 8, 8, CaptureMode::Move, true}}};
    tests.expect(computeClosureLayout(captures, layout, error) && layout.size == 16 && layout.alignment == 8,
        "closure environment is a native aggregate with deterministic Win64 layout");
    tests.expect(callableTrait(captures) == CallableTrait::FnOnce,
        "a move capture makes the closure explicitly FnOnce");
    const std::array<Capture, 1> escaping = {{{"local", 8, 8, CaptureMode::Reference, false}}};
    tests.expect(!validateClosureEscape(escaping, true, error) && validateClosureEscape(escaping, false, error),
        "reference captures cannot escape their source lifetime");
    tests.expect(nonCapturingLambdaConvertsToPointer({}) && !nonCapturingLambdaConvertsToPointer(captures),
        "only non-capturing lambdas convert directly to function pointers");
    tests.expect(!genericLambdaUsesErasedDispatch(),
        "generic lambdas monomorphize and never acquire automatic erased dispatch");
    const auto bound = windowsX64BoundMethodLayout();
    tests.expect(bound.size == 16 && bound.alignment == 8,
        "a bound method exposes receiver plus entry pointer on Windows x64");
    tests.expect(validateMustTail(native, native, false, error) &&
            !validateMustTail(native, c, false, error) &&
            !validateMustTail(native, native, true, error),
        "must_tail is verified against ABI equality and pending cleanup");
    return tests.result();
}
