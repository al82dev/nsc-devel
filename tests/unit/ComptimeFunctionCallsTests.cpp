#include "nativesharp/Frontend/ComptimeEngine.h"

#include <iostream>
#include <memory>
#include <string>

namespace
{
int failures = 0;

// Keep each failure visible while continuing the other independent checks.
void expect(bool condition, const char* message)
{
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}
}

int main()
{
    using namespace nativesharp::frontend;
    const std::string firstSource =
        "int F(int x) { return x + 1; } int Main() { return #run(F(41)); }";
    const std::string changedSource =
        "int F(int x) { return x + 2; } int Main() { return #run(F(41)); }";
    const auto cache = std::make_shared<ComptimeCache>();
    const ComptimeEngine engine({}, cache);
    const auto first = engine.execute(firstSource);
    const auto repeat = engine.execute(firstSource);
    const auto changed = engine.execute(changedSource);
    expect(first.success && first.evaluations.size() == 1 && first.evaluations[0].value == 42,
        "ordinary pure function returns the expected value");
    expect(repeat.success && repeat.cacheHits == 0 && repeat.cacheMisses == 1,
        "function calls are not cached without resource accounting");
    expect(changed.success && changed.evaluations.size() == 1 && changed.evaluations[0].value == 43,
        "changing a function body changes the computed result");
    expect(first.success && changed.success && first.dependencies[0].digest != changed.dependencies[0].digest,
        "function source contributes to dependency identity");

    ComptimeLimits stepLimit;
    stepLimit.maximumSteps = 16;
    const auto steps = ComptimeEngine(stepLimit).execute(firstSource);
    expect(!steps.success && steps.error.find("step budget") != std::string::npos,
        "function evaluation respects the shared step budget");

    ComptimeLimits memoryLimit;
    memoryLimit.maximumMemoryBytes = 4096;
    const auto memory = ComptimeEngine(memoryLimit).execute(firstSource);
    expect(!memory.success && memory.error.find("memory budget") != std::string::npos,
        "temporary parsing observes the memory reservation");

    ComptimeLimits depthLimit;
    depthLimit.maximumNesting = 2;
    const auto depth = ComptimeEngine(depthLimit).execute(firstSource);
    expect(!depth.success && depth.error.find("nesting limit") != std::string::npos,
        "function worklist observes the nesting limit");

    ComptimeExecutionContext context;
    context.cancellationRequested = [] { return true; };
    const auto cancelled = ComptimeEngine({}, {}, context).execute(firstSource);
    expect(!cancelled.success && cancelled.error.find("cancelled") != std::string::npos,
        "cancellation remains observable");

    std::size_t polls = 0;
    ComptimeExecutionContext laterCancellation;
    laterCancellation.cancellationRequested = [&polls] { return ++polls > 120; };
    const auto cancelledDuringWork = ComptimeEngine({}, {}, laterCancellation).execute(
        "int F() { return F(); } int Main() { return #run(F()); }");
    expect(!cancelledDuringWork.success && polls > 120 &&
           cancelledDuringWork.error.find("cancelled") != std::string::npos,
        "cancellation interrupts in-progress function work");

    const auto host = engine.execute("int Main() { return #run(Environment.TickCount64()); }");
    expect(!host.success && host.error.find("sandbox forbids") != std::string::npos,
        "host calls remain forbidden");
    return failures == 0 ? 0 : 1;
}
