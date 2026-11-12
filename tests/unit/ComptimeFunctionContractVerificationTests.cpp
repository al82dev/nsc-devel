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

    const auto cache = std::make_shared<ComptimeCache>();
    const auto repeated = ComptimeEngine({}, cache).execute(nestedSource);
    const auto repeatedAgain = ComptimeEngine({}, cache).execute(nestedSource);
    expect(repeated.success && repeatedAgain.success &&
           repeated.cacheHits == 0 && repeatedAgain.cacheHits == 0,
        "function calls do not reuse an unaccounted cached value");
    return failures == 0 ? 0 : 1;
}
