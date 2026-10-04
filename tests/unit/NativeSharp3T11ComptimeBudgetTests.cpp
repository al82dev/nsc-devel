#include "nativesharp/Frontend/ComptimeEngine.h"

#include "../support/TestContext.h"

#include <string>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::frontend;

    ComptimeLimits steps;
    steps.maximumSteps = 5;
    const ComptimeResult stepFailure = ComptimeEngine(steps).execute(
        "int F(){ return #run(1 + 2 + 3 + 4 + 5 + 6); }");
    const ComptimeResult repeatedStepFailure = ComptimeEngine(steps).execute(
        "int F(){ return #run(1 + 2 + 3 + 4 + 5 + 6); }");
    tests.expect(!stepFailure.success && stepFailure.error.find("step budget exceeded") !=
            std::string::npos && stepFailure.error == repeatedStepFailure.error,
        "step exhaustion produces a reproducible diagnostic");

    ComptimeLimits memory;
    memory.maximumSourceBytes = 1024;
    memory.maximumMemoryBytes = 32;
    const ComptimeResult memoryFailure = ComptimeEngine(memory).execute(std::string(64, ' '));
    tests.expect(!memoryFailure.success && memoryFailure.error ==
            "comptime memory budget exceeded by source buffer",
        "source and evaluation storage are charged to a deterministic memory budget");

    ComptimeLimits nesting;
    nesting.maximumNesting = 2;
    const ComptimeResult nestingFailure = ComptimeEngine(nesting).execute(
        "int F(){ return #run((((1)))); }");
    tests.expect(!nestingFailure.success && nestingFailure.error.find("nesting limit exceeded") !=
            std::string::npos,
        "deep compile-time recursion/nesting stops at its configured bound");

    const ComptimeResult cancelled = ComptimeEngine({}, {},
        {{}, true, [] { return true; }}).execute("int F(){ return #run(40 + 2); }");
    tests.expect(!cancelled.success && cancelled.error == "comptime evaluation cancelled",
        "cancellation is observed before executing compile-time work");

    ComptimeLimits macros;
    macros.maximumMacroExpansions = 3;
    const ComptimeResult recursiveMacro = ComptimeEngine(macros).execute(
        "#macro again(v){ #expand again($v) } int F(){ #expand again(1) }");
    tests.expect(!recursiveMacro.success &&
            recursiveMacro.error.find("macro expansion limit exceeded") != std::string::npos &&
            recursiveMacro.error.find("#expand again") != std::string::npos,
        "recursive macros stop with a bounded deterministic expansion backtrace");

    const ComptimeResult unsafeOperation = ComptimeEngine().execute(
        "int F(){ return #run(unsafe_call()); }");
    tests.expect(!unsafeOperation.success && unsafeOperation.error.find("sandbox forbids") !=
            std::string::npos,
        "unsafe calls cannot execute in the semantic sandbox");
    const ComptimeResult ffiCapability = ComptimeEngine().execute(
        "#capability(\"ffi\", \"Demo.Entry\") int F(){ return 0; }");
    tests.expect(!ffiCapability.success && ffiCapability.error.find("hermetic") !=
            std::string::npos,
        "FFI is rejected before any host code can run or crash the compiler process");

    return tests.result();
}
