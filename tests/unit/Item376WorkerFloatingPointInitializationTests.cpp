#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/LLVM/FloatingPointMode.h"

#include "../support/TestContext.h"

#include <array>
#include <latch>

int main()
{
    using namespace nativesharp::driver;
    using namespace nativesharp::llvm_backend;
    nativesharp::tests::TestContext tests;

    FloatingPointModeScope submittingThreadScope;
    submittingThreadScope.set(FloatingPointMode::Deterministic);
    submittingThreadScope.setFmaContractPolicy(FmaContractPolicy::Off);
    submittingThreadScope.setRoundingMode(FloatingPointRoundingMode::Downward);
    submittingThreadScope.setDenormalMode(
        FloatingPointDenormalMode::FlushToZeroAndDenormalsAreZero);
    const FloatingPointState submittedState = captureFloatingPointState();

    CompilationWorkerPool pool(3, [submittedState](std::size_t) {
        initializeFloatingPointStateForWorker(submittedState);
    });

    std::array<FloatingPointState, 3> observed{};
    std::latch allStarted(3);
    std::latch release(1);
    for (std::size_t task = 0; task < observed.size(); ++task)
    {
        pool.submit([&](std::size_t workerIndex) {
            observed[workerIndex] = captureFloatingPointState();
            allStarted.count_down();
            release.wait();
        });
    }
    allStarted.wait();
    release.count_down();
    pool.wait();

    for (std::size_t worker = 0; worker < observed.size(); ++worker)
    {
        tests.expect(observed[worker] == submittedState,
            "worker " + std::to_string(worker) +
                " starts with the submitting floating-point contract");
    }
    tests.expect(captureFloatingPointState() == submittedState,
        "worker initialization does not alter the submitting thread");
    return tests.result();
}
