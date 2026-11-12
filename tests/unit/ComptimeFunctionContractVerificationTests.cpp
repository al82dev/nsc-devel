#include "nativesharp/Frontend/ComptimeEngine.h"

#include <iostream>
#include <memory>
#include <string>

namespace
{
int failures = 0;

// Report every independent contract failure in the same directed execution.
void expect(bool condition, const char* message)
{
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

bool contains(const std::string& text, const char* fragment)
{
    return text.find(fragment) != std::string::npos;
}
}

int main()
{
    using namespace nativesharp::frontend;
    const std::string nestedSource =
        "int Twice(int value) { return value * 2; } "
        "int Add(int left, int right) { return left + right; } "
        "int Main() { return #run(Add(20, Twice(11))); }";
    const ComptimeEngine engine;
    const auto nested = engine.execute(nestedSource);
    expect(nested.success && nested.evaluations.size() == 1,
        "nested pure calls produce one evaluation");
    expect(nested.success && nested.evaluations.size() == 1 &&
           nested.evaluations[0].value == 42,
        "arguments and nested return values are propagated");

    const std::string firstBody =
        "int Value(int input) { return input + 1; } "
        "int Main() { return #run(Value(41)); }";
    const std::string changedBody =
        "int Value(int input) { return input - 1; } "
        "int Main() { return #run(Value(41)); }";
    const auto first = engine.execute(firstBody);
    const auto changed = engine.execute(changedBody);
    expect(first.success && first.evaluations.size() == 1 &&
           first.evaluations[0].value == 42,
        "the first function body is evaluated");
    expect(changed.success && changed.evaluations.size() == 1 &&
           changed.evaluations[0].value == 40,
        "a changed function body changes the result");
    expect(first.success && changed.success && first.dependencies.size() == 1 &&
           changed.dependencies.size() == 1 &&
           first.dependencies[0].digest != changed.dependencies[0].digest,
        "a changed function body changes the dependency digest");

    ComptimeLimits exactSteps;
    exactSteps.maximumSteps = nested.executedSteps;
    const auto atStepLimit = ComptimeEngine(exactSteps).execute(nestedSource);
    expect(atStepLimit.success, "the exact measured step budget succeeds");
    if (nested.executedSteps != 0)
    {
        exactSteps.maximumSteps = nested.executedSteps - 1;
        const auto belowStepLimit = ComptimeEngine(exactSteps).execute(nestedSource);
        expect(!belowStepLimit.success && contains(belowStepLimit.error, "step budget"),
            "one step below the measured budget is rejected");
    }

    ComptimeLimits exactMemory;
    exactMemory.maximumMemoryBytes = nested.memoryUsed;
    const auto atMemoryLimit = ComptimeEngine(exactMemory).execute(nestedSource);
    expect(atMemoryLimit.success, "the exact measured accounting budget succeeds");
    if (nested.memoryUsed != 0)
    {
        exactMemory.maximumMemoryBytes = nested.memoryUsed - 1;
        const auto belowMemoryLimit = ComptimeEngine(exactMemory).execute(nestedSource);
        expect(!belowMemoryLimit.success && contains(belowMemoryLimit.error, "memory budget"),
            "one byte below the measured accounting budget is rejected");
    }

    const auto state = engine.execute(
        "int State; int Read() { return State; } int Main() { return #run(Read()); }");
    expect(!state.success && contains(state.error, "non-parameter state"),
        "reading state is rejected with a bounded-profile diagnostic");
    const auto host = engine.execute(
        "int Main() { return #run(Environment.TickCount64()); }");
    expect(!host.success && contains(host.error, "qualified function lookup"),
        "host calls are rejected without executing the host");

    ComptimeExecutionContext cancellation;
    cancellation.cancellationRequested = [] { return true; };
    const auto cancelled = ComptimeEngine({}, {}, cancellation).execute(nestedSource);
    expect(!cancelled.success && contains(cancelled.error, "cancelled"),
        "cancellation is observable before evaluation");
    expect(cancelled.evaluations.empty(),
        "early cancellation does not publish an evaluation");

    // Sweep every observed cancellation point of a unit with two completed evaluations.
    const std::string transactionSource =
        "int A(int x) { return x + 1; } int Main() { return #run(A(20)) + #run(A(21)); }";
    std::size_t polls = 0;
    ComptimeExecutionContext counting;
    counting.cancellationRequested = [&polls] { ++polls; return false; };
    const auto completed = ComptimeEngine({}, {}, counting).execute(transactionSource);
    expect(completed.success && completed.evaluations.size() == 2, "transaction control succeeds");
    const auto totalPolls = polls;
    for (std::size_t stop = 1; stop <= totalPolls; ++stop)
    {
        polls = 0;
        ComptimeExecutionContext cutoff;
        cutoff.cancellationRequested = [&polls, stop] { return ++polls >= stop; };
        const auto result = ComptimeEngine({}, {}, cutoff).execute(transactionSource);
        expect(!result.success && result.error == "comptime evaluation cancelled",
            "every observed cancellation is reported");
        expect(result.source == transactionSource && result.evaluations.empty() &&
               result.dependencies.empty() && result.attributes.empty() &&
               result.macroExpansions.empty() && result.authoredDiagnostics.empty() &&
               result.declaredCapabilities.empty() && result.reproducibilityDigest.empty(),
            "cancellation returns no partial unit result");
    }

    // A cancellation after preprocessing and directive recording must not
    // publish any of those records or the expanded source.
    const std::string directiveSource =
        "#macro make() expression { 40 + 2 } "
        "#capability(\"env\", \"NSC_DUMMY\") "
        "#compile_note(\"notice\", \"NOTE-1\") #[Example] "
        "int x = #expand make(); int Main() { return #run(40 + 2); }";
    std::size_t directivePolls = 0;
    ComptimeExecutionContext directiveCounting;
    directiveCounting.cancellationRequested = [&directivePolls] { ++directivePolls; return false; };
    const auto directivesComplete = ComptimeEngine({}, {}, directiveCounting).execute(directiveSource);
    expect(directivesComplete.success && !directivesComplete.macroExpansions.empty() &&
           !directivesComplete.declaredCapabilities.empty() && !directivesComplete.authoredDiagnostics.empty() &&
           !directivesComplete.attributes.empty() && directivesComplete.evaluations.size() == 1,
        "directive transaction control populates each record category");
    const auto directiveTotalPolls = directivePolls;
    for (std::size_t stop = 1; stop <= directiveTotalPolls; ++stop)
    {
        directivePolls = 0;
        ComptimeExecutionContext cutoff;
        cutoff.cancellationRequested = [&directivePolls, stop] { return ++directivePolls >= stop; };
        const auto result = ComptimeEngine({}, {}, cutoff).execute(directiveSource);
        expect(!result.success && result.error == "comptime evaluation cancelled" &&
               result.source == directiveSource && result.evaluations.empty() &&
               result.dependencies.empty() && result.attributes.empty() && result.macroExpansions.empty() &&
               result.authoredDiagnostics.empty() && result.declaredCapabilities.empty(),
            "cancellation discards directive and macro records");
    }
    const auto arithmeticCache = std::make_shared<ComptimeCache>();
    ComptimeExecutionContext cancelAfterStore;
    cancelAfterStore.cancellationRequested = [arithmeticCache] { return arithmeticCache->size() != 0; };
    const auto cancelledCached = ComptimeEngine({}, arithmeticCache, cancelAfterStore).execute(directiveSource);
    expect(!cancelledCached.success && cancelledCached.error == "comptime evaluation cancelled" &&
           cancelledCached.source == directiveSource && cancelledCached.evaluations.empty() &&
           cancelledCached.dependencies.empty() && cancelledCached.attributes.empty() &&
           cancelledCached.macroExpansions.empty() && cancelledCached.authoredDiagnostics.empty() &&
           cancelledCached.declaredCapabilities.empty(),
        "cancellation after a cached arithmetic evaluation publishes no unit output");
    const auto resumedCached = ComptimeEngine({}, arithmeticCache).execute(directiveSource);
    expect(resumedCached.success && resumedCached.evaluations.size() == 1 &&
           resumedCached.evaluations[0].value == 42,
        "later evaluation with the shared cache still produces the correct value");

    const auto cache = std::make_shared<ComptimeCache>();
    const auto repeated = ComptimeEngine({}, cache).execute(nestedSource);
    const auto repeatedAgain = ComptimeEngine({}, cache).execute(nestedSource);
    expect(repeated.success && repeatedAgain.success &&
           repeated.cacheHits == 0 && repeatedAgain.cacheHits == 0,
        "function calls do not reuse an unaccounted cached value");
    return failures == 0 ? 0 : 1;
}
