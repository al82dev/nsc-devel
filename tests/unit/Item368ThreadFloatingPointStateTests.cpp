#include "nativesharp/LLVM/FloatingPointMode.h"

#include "../support/TestContext.h"

#include <atomic>
#include <thread>

int main()
{
    using namespace nativesharp::llvm_backend;
    nativesharp::tests::TestContext tests;
    const FloatingPointState initial = captureFloatingPointState();
    bool workerStartedWithDefaults = false;
    bool workerKeptItsState = false;
    std::atomic<bool> workerConfigured{false};
    std::atomic<bool> mainObserved{false};

    {
        FloatingPointModeScope mainScope;
        mainScope.set(FloatingPointMode::Deterministic);
        mainScope.setFmaContractPolicy(FmaContractPolicy::Off);
        mainScope.setRoundingMode(FloatingPointRoundingMode::NearestEven);
        mainScope.setDenormalMode(FloatingPointDenormalMode::Preserve);

        std::thread worker([&] {
            workerStartedWithDefaults = captureFloatingPointState() == FloatingPointState{};
            FloatingPointModeScope workerScope;
            workerScope.set(FloatingPointMode::Fast);
            workerScope.setFmaContractPolicy(FmaContractPolicy::Fast);
            workerScope.setRoundingMode(FloatingPointRoundingMode::TowardZero);
            workerScope.setDenormalMode(FloatingPointDenormalMode::FlushToZeroAndDenormalsAreZero);
            workerConfigured.store(true, std::memory_order_release);
            while (!mainObserved.load(std::memory_order_acquire)) std::this_thread::yield();
            const FloatingPointState workerState = captureFloatingPointState();
            workerKeptItsState = workerState.mode == FloatingPointMode::Fast &&
                workerState.roundingMode == FloatingPointRoundingMode::TowardZero &&
                workerState.denormalMode == FloatingPointDenormalMode::FlushToZeroAndDenormalsAreZero;
        });
        while (!workerConfigured.load(std::memory_order_acquire)) std::this_thread::yield();
        const FloatingPointState mainState = captureFloatingPointState();
        tests.expect(mainState.mode == FloatingPointMode::Deterministic &&
                mainState.roundingMode == FloatingPointRoundingMode::NearestEven &&
                mainState.denormalMode == FloatingPointDenormalMode::Preserve,
            "worker policy changes do not leak into the submitting thread");
        mainObserved.store(true, std::memory_order_release);
        worker.join();
    }

    tests.expect(workerStartedWithDefaults, "a new thread starts with the strict default state");
    tests.expect(workerKeptItsState, "a worker retains its own complete floating-point state");
    tests.expect(captureFloatingPointState() == initial,
        "scope destruction restores the complete calling-thread state");
    return tests.result();
}
