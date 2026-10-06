#include "nativesharp/Frontend/ProjectCallableAnalysisResult.h"
#include "nativesharp/Frontend/ProjectCallableWaveScheduler.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "../support/TestContext.h"

#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
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
}

int main()
{
    using namespace nativesharp::frontend;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const std::vector<std::size_t> callables{8, 3, 5};
    std::vector<ProjectCallableAnalysisResult> serialResults;
    std::string error;
    ProjectCallableWaveScheduler serial(1);
    const bool serialOk = serial.run(
        callables, 4, makeResult, {}, serialResults, error);
    tests.expect(serialOk && error.empty() && serialResults.size() == 3,
        "j1 executes the complete wave synchronously");
    tests.expect(serialResults[0].callableOrdinal() == 8 &&
            serialResults[1].callableOrdinal() == 3 &&
            serialResults[2].callableOrdinal() == 5,
        "j1 preserves stable input slots");

    std::vector<ProjectCallableAnalysisResult> parallelResults;
    ProjectCallableWaveScheduler parallel(2);
    const bool parallelOk = parallel.run(
        callables, 4, makeResult, {}, parallelResults, error);
    tests.expect(parallelOk && parallelResults.size() == serialResults.size(),
        "j2 completes the same wave");
    bool sameSlots = parallelResults.size() == serialResults.size();
    for (std::size_t index = 0; sameSlots && index < serialResults.size(); ++index)
        sameSlots = parallelResults[index].callableOrdinal() ==
            serialResults[index].callableOrdinal();
    tests.expect(sameSlots, "parallel completion publishes only through stable slots");

    std::vector<ProjectCallableAnalysisResult> cancelledResults;
    std::atomic_bool cancel = true;
    const bool cancelled = parallel.run(
        callables, 0, makeResult,
        [&] { return cancel.load(); }, cancelledResults, error);
    tests.expect(!cancelled && cancelledResults.empty() && !error.empty(),
        "cancellation publishes no partial wave");

    std::vector<ProjectCallableAnalysisResult> failedResults;
    const bool failed = parallel.run(
        callables, 0,
        [](std::size_t callable, std::size_t wave) {
            if (callable == 3) throw std::runtime_error("synthetic failure");
            return makeResult(callable, wave);
        },
        {}, failedResults, error);
    tests.expect(!failed && failedResults.empty() && !error.empty(),
        "worker failure publishes no partial wave");
    const bool serialFailed = serial.run(
        callables, 0,
        [](std::size_t, std::size_t) -> ProjectCallableAnalysisResult {
            throw std::runtime_error("synthetic serial failure");
        },
        {}, failedResults, error);
    tests.expect(!serialFailed && failedResults.empty() && !error.empty(),
        "j1 failure uses the same non-publishing contract as parallel execution");
    tests.expect(serial.requestedWorkerCount() == 1 &&
            parallel.requestedWorkerCount() == 2,
        "scheduler retains the selected bounded worker count");
    return tests.result();
}
