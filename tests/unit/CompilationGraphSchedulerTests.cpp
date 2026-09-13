#include "nativesharp/Driver/CompilationGraphScheduler.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

int main()
{
    using namespace nativesharp::driver;
    nativesharp::tests::TestContext tests;
    CompilationWorkerPool pool(4);
    CompilationGraphScheduler scheduler;
    std::atomic<unsigned> roots = 0;
    std::atomic<bool> dependentSawRoots = false;

    const std::size_t z = scheduler.addTask("z-root", {}, [&](std::size_t) { ++roots; });
    const std::size_t a = scheduler.addTask("a-root", {}, [&](std::size_t) { ++roots; });
    const std::array<std::size_t, 2> dependencies = {z, a};
    const std::size_t child = scheduler.addTask("child", dependencies, [&](std::size_t) {
        dependentSawRoots = roots == 2;
    });
    std::string error;
    tests.expect(scheduler.run(pool, error), "an acyclic compilation graph is scheduled");
    const std::array<std::size_t, 3> expected = {a, z, child};
    tests.expect(std::ranges::equal(scheduler.submissionOrder(), expected),
        "ready tasks are submitted by stable key and then ordinal");
    tests.expect(dependentSawRoots, "a dependent wave starts only after all prerequisites finish");
    tests.expect(error.empty(), "successful scheduling keeps its diagnostic empty");

    bool rejectedForward = false;
    try
    {
        CompilationGraphScheduler invalid;
        const std::array<std::size_t, 1> forward = {0};
        invalid.addTask("self", forward, [](std::size_t) {});
    }
    catch (const std::invalid_argument&)
    {
        rejectedForward = true;
    }
    tests.expect(rejectedForward,
        "dependencies must name already established deterministic ordinals");
    return tests.result();
}
