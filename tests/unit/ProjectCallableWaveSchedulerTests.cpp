#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Frontend/ProjectCallableAnalysisResult.h"
#include "nativesharp/Frontend/ProjectCallableWaveScheduler.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "../support/TestContext.h"

#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{
nativesharp::frontend::ProjectCallableAnalysisResult makeResult(
    std::size_t callable,
    std::size_t wave)
{
    return {callable, wave,
        std::make_unique<nativesharp::frontend::ProjectMethodBodyModel>(),
        nativesharp::diagnostics::DiagnosticEngine()};
}

using nativesharp::frontend::ProjectCallableAnalysisResult;
using nativesharp::frontend::ProjectCallableWaveScheduler;
using nativesharp::tests::TestContext;

void testStableSlots(TestContext& tests)
{
    const std::vector<std::size_t> callables{8, 3, 5};
    std::vector<ProjectCallableAnalysisResult> serialResults;
    std::vector<ProjectCallableAnalysisResult> parallelResults;
    std::string error;
    ProjectCallableWaveScheduler serial(1);
    ProjectCallableWaveScheduler parallel(2);
    const bool serialOk = serial.run(callables, 4, makeResult, {}, serialResults, error);
    const bool parallelOk = parallel.run(callables, 4, makeResult, {}, parallelResults, error);
    tests.expect(serialOk && parallelOk && serialResults.size() == 3,
        "j1 and j2 execute the complete wave");
    bool sameSlots = parallelResults.size() == serialResults.size();
    for (std::size_t index = 0; sameSlots && index < serialResults.size(); ++index)
        sameSlots = parallelResults[index].callableOrdinal() ==
            serialResults[index].callableOrdinal();
    tests.expect(sameSlots && serialResults[0].callableOrdinal() == 8,
        "parallel completion publishes only through stable input slots");
    tests.expect(serial.requestedWorkerCount() == 1 &&
            parallel.requestedWorkerCount() == 2,
        "scheduler retains the selected bounded worker count");
}

void testCancellation(TestContext& tests)
{
    const std::vector<std::size_t> callables{8, 3, 5};
    std::vector<ProjectCallableAnalysisResult> results;
    std::string error;
    ProjectCallableWaveScheduler scheduler(2);
    std::atomic_size_t analyses = 0;
    const bool cancelled = scheduler.run(
        callables, 0,
        [&](std::size_t callable, std::size_t wave) {
            ++analyses;
            return makeResult(callable, wave);
        },
        [] { return true; }, results, error);
    tests.expect(!cancelled && results.empty() && analyses == 0,
        "pre-existing cancellation schedules no callable analysis");

    std::atomic_bool cancelAfterAnalysis = false;
    const bool lateCancelled = scheduler.run(
        callables, 0,
        [&](std::size_t callable, std::size_t wave) {
            if (++analyses == callables.size()) cancelAfterAnalysis = true;
            return makeResult(callable, wave);
        },
        [&] { return cancelAfterAnalysis.load(); }, results, error);
    tests.expect(!lateCancelled && results.empty(),
        "cancellation observed after analysis prevents wave publication");
}

void testFailureParity(TestContext& tests)
{
    const std::vector<std::size_t> callables{8, 3, 5};
    std::vector<ProjectCallableAnalysisResult> results;
    std::string serialError;
    std::string parallelError;
    const auto fail = [](std::size_t callable, std::size_t wave) {
        if (callable == 3) throw std::runtime_error("synthetic failure");
        return makeResult(callable, wave);
    };
    ProjectCallableWaveScheduler serial(1);
    ProjectCallableWaveScheduler parallel(2);
    const bool serialOk = serial.run(callables, 0, fail, {}, results, serialError);
    const bool parallelOk = parallel.run(callables, 0, fail, {}, results, parallelError);
    tests.expect(!serialOk && !parallelOk && results.empty(),
        "j1 and j2 failures publish no partial wave");
    tests.expect(serialError == parallelError &&
            serialError == "callable analysis failed: synthetic failure",
        "j1 and j2 expose the same deterministic failure text");
}

void testPoolReuseAndNestedFastPath(TestContext& tests)
{
    const std::vector<std::size_t> callables{8, 3, 5};
    std::vector<ProjectCallableAnalysisResult> results;
    std::string error;
    ProjectCallableWaveScheduler scheduler(2);
    const bool first = scheduler.run(callables, 0, makeResult, {}, results, error);
    const bool second = scheduler.run(callables, 1, makeResult, {}, results, error);
    tests.expect(first && second && results.front().waveOrdinal() == 1,
        "one scheduler reuses its bounded pool across waves");

    nativesharp::driver::CompilationWorkerPool outer(1);
    bool nestedOk = false;
    outer.submit([&](std::size_t) {
        nestedOk = scheduler.run(callables, 2, makeResult, {}, results, error);
    });
    outer.wait();
    tests.expect(nestedOk && results.size() == callables.size(),
        "invocation from a compilation worker uses the synchronous fast path");
}

void testConcurrentRunIsRejected(TestContext& tests)
{
    const std::vector<std::size_t> callables{8, 3, 5};
    ProjectCallableWaveScheduler scheduler(2);
    std::atomic_size_t entered = 0;
    std::atomic_bool release = false;
    std::vector<ProjectCallableAnalysisResult> firstResults;
    std::string firstError;
    bool firstOk = false;
    std::thread first([&] {
        firstOk = scheduler.run(
            callables, 0,
            [&](std::size_t callable, std::size_t wave) {
                ++entered;
                while (!release.load()) std::this_thread::yield();
                return makeResult(callable, wave);
            },
            {}, firstResults, firstError);
    });
    while (entered.load() == 0) std::this_thread::yield();
    std::vector<ProjectCallableAnalysisResult> secondResults;
    std::string secondError;
    const bool secondOk = scheduler.run(
        callables, 1, makeResult, {}, secondResults, secondError);
    release = true;
    first.join();
    tests.expect(firstOk && entered == callables.size(),
        "reentrant analysis callback completes on isolated slots");
    tests.expect(!secondOk && secondResults.empty() &&
            secondError == "callable analysis scheduler is already running",
        "a second coordinator cannot reenter the persistent pool");
}

void testEmptyCallback(TestContext& tests)
{
    ProjectCallableWaveScheduler scheduler(1);
    std::vector<ProjectCallableAnalysisResult> results;
    std::string error;
    const std::vector<std::size_t> callables{8};
    const bool ok = scheduler.run(callables, 0, {}, {}, results, error);
    tests.expect(!ok && results.empty() && !error.empty(),
        "empty analysis callback is rejected without publishing state");
}
}

int main()
{
    TestContext tests;
    testStableSlots(tests);
    testCancellation(tests);
    testFailureParity(tests);
    testPoolReuseAndNestedFastPath(tests);
    testConcurrentRunIsRejected(tests);
    testEmptyCallback(tests);
    return tests.result();
}
