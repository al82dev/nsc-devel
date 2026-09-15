#include "nativesharp/Frontend/ComptimeEngine.h"

#include <future>
#include <iostream>
#include <memory>
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
    constexpr std::string_view source =
        "comptime int Main() { return #run((0x20 + 10) ^ 0); }";

    const auto cold = ComptimeEngine({}, std::make_shared<ComptimeCache>()).execute(source);
    const auto shared = std::make_shared<ComptimeCache>();
    const ComptimeEngine warmEngine({}, shared);
    const auto populate = warmEngine.execute(source);
    const auto warm = warmEngine.execute(source);
    expect(cold.success && populate.success && warm.success,
        "cold and warm executions succeed");
    expect(cold.source == warm.source &&
           cold.reproducibilityDigest == warm.reproducibilityDigest,
        "cache state is excluded from the canonical result");
    expect(cold.reproducibilityDigest.size() == 64,
        "the canonical result is identified by SHA-256");

    std::vector<std::future<ComptimeResult>> tasks;
    for (int index = 0; index < 8; ++index)
        tasks.push_back(std::async(std::launch::async, [shared, source] {
            return ComptimeEngine({}, shared).execute(source);
        }));
    for (auto& task : tasks)
    {
        const auto parallel = task.get();
        expect(parallel.success &&
               parallel.reproducibilityDigest == cold.reproducibilityDigest &&
               parallel.source == cold.source,
            "parallel scheduling cannot change comptime output");
    }

    const auto changed = ComptimeEngine().execute(
        "comptime int Main() { return #run((0x20 + 11) ^ 0); }");
    expect(changed.success && changed.reproducibilityDigest != cold.reproducibilityDigest,
        "a changed input changes the reproducibility identity");
    return failures == 0 ? 0 : 1;
}
