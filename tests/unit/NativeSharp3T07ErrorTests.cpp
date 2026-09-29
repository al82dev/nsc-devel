#include "nativesharp/Language/ErrorSemantics.h"

#include "../support/TestContext.h"

#include <array>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;
    const ResultProtocol result{"type:Packet", "type:IoError"};
    tests.expect(supportsQuestionPropagation(result, error),
        "? accepts a complete static Result protocol without runtime identity");
    ResultProtocol incomplete = result;
    incomplete.hasTakeError = false;
    tests.expect(!supportsQuestionPropagation(incomplete, error),
        "? rejects incomplete protocols before lowering");
    tests.expect(errorConversionIsExplicit("E", "E", false) &&
            errorConversionIsExplicit("E1", "E2", true) &&
            !errorConversionIsExplicit("E1", "E2", false),
        "error propagation never invents an implicit cross-error conversion");

    tests.expect(validateThrowContract({"type:IoError", ThrowAbi::DualReturn, false, false}, error) &&
            !validateThrowContract({"type:IoError", ThrowAbi::DualReturn, true, false}, error),
        "typed throws has a native ABI and requires adapters at C boundaries");
    tests.expect(catchesAllDeclaredErrors(
            std::array<std::string, 2>{"Io", "Parse"},
            std::array<std::string, 2>{"Parse", "Io"}, false) &&
            !catchesAllDeclaredErrors(
                std::array<std::string, 2>{"Io", "Parse"},
                std::array<std::string, 1>{"Io"}, false),
        "typed catch coverage is checked without managed RTTI");
    tests.expect(neverCoercesTo("builtin:int") && neverCoercesTo("type:Packet"),
        "never coerces to every inhabited result type without producing a value");
    tests.expect(unifyLoopBreakTypes(std::array<std::string, 3>{"never", "builtin:int", "builtin:int"}) ==
            "builtin:int" && !unifyLoopBreakTypes(
                std::array<std::string, 2>{"builtin:int", "builtin:long"}).has_value(),
        "loop expressions unify break values while ignoring diverging paths");
    const PanicContract abort = panicContract(PanicProfile::Abort);
    const PanicContract unwind = panicContract(PanicProfile::NativeUnwind);
    tests.expect(!abort.runsCleanup && unwind.runsCleanup && !abort.mayCrossFfi && !unwind.mayCrossFfi,
        "panic profiles define cleanup and forbid implicit unwind across FFI");
    tests.expect(assertionRequiresUnsafe(AssertionKind::Assume) &&
            !assertionRequiresUnsafe(AssertionKind::Assert),
        "assume is unsafe because a false premise may introduce undefined behavior");
    return tests.result();
}
