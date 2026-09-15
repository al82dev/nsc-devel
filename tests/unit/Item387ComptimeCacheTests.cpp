#include "nativesharp/Frontend/ComptimeEngine.h"

#include <iostream>
#include <memory>

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
    return failures == 0 ? 0 : 1;
}
