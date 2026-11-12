#include "nativesharp/Frontend/ComptimeEngine.h"

#include <atomic>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

namespace
{
int failures = 0;
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
    const auto cache = std::make_shared<ComptimeCache>();
    const ComptimeEngine engine({}, cache);
    const auto first = engine.execute("int Main() { return #run(40 + 2); }");
    const auto second = engine.execute("int Main() { return #run(40 + 2); }");
    expect(first.success && first.cacheMisses == 1 && first.cacheHits == 0,
        "the first execution populates the cache");
    expect(second.success && second.cacheHits == 1 && second.cacheMisses == 0,
        "an identical dependency is served from cache");
    expect(first.executedSteps == second.executedSteps,
        "cache hits retain their resource cost");
    expect(first.dependencies.size() == 1 &&
           first.dependencies.front().digest == second.dependencies.front().digest,
        "the dependency manifest uses a stable content digest");

    const auto changed = engine.execute("int Main() { return #run(41 + 2); }");
    expect(changed.success && changed.cacheMisses == 1 && changed.evaluations.front().value == 43,
        "changed source invalidates the cached result");
    expect(changed.dependencies.front().digest != first.dependencies.front().digest,
        "dependency identity changes with expression content");
    expect(cache->size() == 2, "only distinct expression dependencies occupy cache entries");

    const std::string nested = "int Main() { return #run(((40 + 2))); }";
    ComptimeLimits high;
    high.maximumNesting = 8;
    ComptimeLimits boundary;
    boundary.maximumNesting = 2;
    ComptimeLimits low;
    low.maximumNesting = 1;
    const auto nestedCache = std::make_shared<ComptimeCache>();
    const auto coldLow = ComptimeEngine(low, std::make_shared<ComptimeCache>()).execute(nested);
    const auto warmHigh = ComptimeEngine(high, nestedCache).execute(nested);
    const auto hitBoundary = ComptimeEngine(boundary, nestedCache).execute(nested);
    const auto warmLow = ComptimeEngine(low, nestedCache).execute(nested);
    expect(!coldLow.success && coldLow.error.find("nesting limit") != std::string::npos,
        "a cold low limit rejects nested parentheses");
    expect(warmHigh.success && hitBoundary.success && hitBoundary.cacheHits == 1 &&
           warmHigh.executedSteps == hitBoundary.executedSteps,
        "a cache hit at the exact nesting boundary preserves the step cost");
    expect(!warmLow.success && warmLow.error == coldLow.error && warmLow.cacheHits == 0,
        "a cached result cannot bypass a stricter nesting limit");

    const auto unknownCache = std::make_shared<ComptimeCache>();
    unknownCache->store(warmHigh.dependencies.front().digest, {42, warmHigh.executedSteps, std::nullopt});
    const auto unknownDepth = ComptimeEngine(high, unknownCache).execute(nested);
    expect(unknownDepth.success && unknownDepth.cacheHits == 0 && unknownDepth.cacheMisses == 1,
        "an entry without depth evidence falls back to evaluation");
    ComptimeLimits zero;
    zero.maximumNesting = 0;
    const auto zeroCache = std::make_shared<ComptimeCache>();
    const auto zeroWarm = ComptimeEngine(high, zeroCache).execute("int Main() { return #run(42); }");
    const auto zeroHit = ComptimeEngine(zero, zeroCache).execute("int Main() { return #run(42); }");
    expect(zeroWarm.success && zeroHit.success && zeroHit.cacheHits == 1,
        "an expression without nested parentheses is reusable at a zero nesting limit");

    ComptimeLimits lowSteps = low;
    lowSteps.maximumSteps = 1;
    const auto coldBoth = ComptimeEngine(lowSteps, std::make_shared<ComptimeCache>()).execute(nested);
    const auto warmBoth = ComptimeEngine(lowSteps, nestedCache).execute(nested);
    expect(!coldBoth.success && !warmBoth.success && coldBoth.error == warmBoth.error,
        "a depth-ineligible entry preserves cold error precedence with a step limit");

    std::atomic<int> errors{0};
    std::vector<std::thread> workers;
    for (int index = 0; index < 4; ++index)
    {
        workers.emplace_back([&, index]
        {
            const bool restricted = (index % 2) == 0;
            const auto result = ComptimeEngine(restricted ? low : boundary, nestedCache).execute(nested);
            if (restricted ? result.success : (!result.success || result.cacheHits != 1)) ++errors;
        });
    }
    for (std::thread& worker : workers) worker.join();
    expect(errors == 0 && nestedCache->size() == 1,
        "shared entries preserve per-execution nesting limits across workers");
    return failures == 0 ? 0 : 1;
}
