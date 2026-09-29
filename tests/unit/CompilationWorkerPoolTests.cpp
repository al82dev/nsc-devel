#include "nativesharp/Driver/CompilationJobPolicy.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"

#include "../support/TestContext.h"

#include <atomic>
#include <latch>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>

int main()
{
    using namespace nativesharp::driver;
    nativesharp::tests::TestContext tests;

    CompilationJobPolicy policy(8);
    policy.setRequestedJobs(4);
    policy.setReservedCores(1);
    CompilationJobPolicyScope policyScope(policy);

    CompilationWorkerPool pool;
    tests.expect(pool.workerCount() == 4,
        "the pool resolves its size from the active compilation job policy");

    std::latch allStarted(4);
    std::latch release(1);
    std::mutex indicesMutex;
    std::set<std::size_t> indices;
    std::atomic<std::size_t> completed = 0;
    std::atomic<bool> workerIndicesCorrect = true;
    std::atomic<bool> inheritedPolicyCorrect = true;
    for (std::size_t task = 0; task < 4; ++task)
    {
        pool.submit([&](std::size_t workerIndex) {
            if (currentCompilationWorkerIndex() != workerIndex) workerIndicesCorrect = false;
            if (activeCompilationJobPolicy() == nullptr ||
                activeCompilationJobPolicy()->effectiveJobs() != 4)
            {
                inheritedPolicyCorrect = false;
            }
            {
                const std::lock_guard lock(indicesMutex);
                indices.insert(workerIndex);
            }
            allStarted.count_down();
            release.wait();
            ++completed;
        });
    }
    allStarted.wait();
    release.count_down();
    pool.wait();
    tests.expect(workerIndicesCorrect,
        "a task observes the stable index of its worker");
    tests.expect(inheritedPolicyCorrect,
        "workers inherit the resolved compilation job policy");
    tests.expect(completed == 4 && indices.size() == 4,
        "the fixed pool can execute one blocking task on every worker");

    const std::size_t first = pool.submit([](std::size_t) {
        throw std::runtime_error("first submitted failure");
    });
    const std::size_t second = pool.submit([](std::size_t) {
        throw std::runtime_error("second submitted failure");
    });
    tests.expect(second == first + 1,
        "task ordinals are monotonic in submission order");
    bool reportedFirst = false;
    try
    {
        pool.wait();
    }
    catch (const std::runtime_error& exception)
    {
        reportedFirst = std::string(exception.what()) == "first submitted failure";
    }
    tests.expect(reportedFirst,
        "wait reports the lowest submitted failure regardless of completion order");

    CompilationWorkerPool cancelOnFailure(1);
    std::atomic<std::size_t> uselessWork = 0;
    cancelOnFailure.submit([](std::size_t) {
        throw std::runtime_error("stop this wave");
    });
    for (std::size_t task = 0; task < 32; ++task)
        cancelOnFailure.submit([&](std::size_t) { ++uselessWork; });
    bool cancellationFailureReported = false;
    try
    {
        cancelOnFailure.wait();
    }
    catch (const std::runtime_error& exception)
    {
        cancellationFailureReported = std::string(exception.what()) == "stop this wave";
    }
    tests.expect(cancellationFailureReported && uselessWork == 0 &&
            cancelOnFailure.discardedTaskCount() == 32,
        "the first fatal task cancels every queued unit in its wave");
    tests.expect(!cancelOnFailure.cancellationRequested(),
        "wait closes the cancellation epoch so the bounded pool can be reused");

    cancelOnFailure.submit([&](std::size_t) { ++uselessWork; });
    cancelOnFailure.wait();
    tests.expect(uselessWork == 1,
        "a canceled pool accepts a later independent wave");

    pool.submit([&](std::size_t) { ++completed; });
    pool.wait();
    tests.expect(completed == 5,
        "the pool remains reusable after a reported task failure");

    bool rejectedEmpty = false;
    try
    {
        pool.submit({});
    }
    catch (const std::invalid_argument&)
    {
        rejectedEmpty = true;
    }
    tests.expect(rejectedEmpty, "the pool rejects an empty task before queueing it");

    return tests.result();
}
