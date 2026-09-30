#include "nativesharp/Language/CoroutineSemantics.h"

#include "../support/TestContext.h"

#include <array>
#include <filesystem>

int main(int argc, char** argv)
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    tests.expect(nativeFutureStart() == FutureStart::Lazy &&
            !asyncFrameHasImplicitHeap(FrameStorage::CallerStack) &&
            !asyncFrameHasImplicitHeap(FrameStorage::ExplicitHeap),
        "native futures are lazy and storage policy belongs to the caller/runtime");
    tests.expect(borrowMayCrossAwait(true, true) && !borrowMayCrossAwait(true, false) &&
            !borrowMayCrossAwait(false, true),
        "borrow across await requires lifetime and address stability proofs");

    std::string error;
    FutureState future;
    tests.expect(future.poll(false, error) && future.state() == PollState::Pending &&
            future.poll(true, error) && future.consume(error) && future.state() == PollState::Consumed,
        "Future poll transitions Created/Pending to one consumable Ready result");
    tests.expect(!future.poll(true, error) && !future.cancel(error),
        "a consumed future cannot be double-resumed or cancelled");
    FutureState cancelled;
    tests.expect(cancelled.poll(false, error) && cancelled.cancel(error) &&
            cancelled.state() == PollState::Cancelled,
        "cancellation destroys a pending future without thread abort");

    const auto cleanup = cancellationCleanup(
        std::array<std::string, 2>{"socket", "buffer"},
        std::array<std::string, 2>{"trace", "unlock"});
    tests.expect(cleanup == std::vector<std::string>({"unlock", "trace", "buffer", "socket"}),
        "cancellation runs active defer and local cleanup in deterministic reverse order");
    tests.expect(asyncGeneratorMayProduce(AsyncGeneratorState::Pending, true) &&
            !asyncGeneratorMayProduce(AsyncGeneratorState::Pending, false),
        "async generator output observes explicit consumer backpressure");

    tests.expect(structurallySend({true, true, false, false, false}) &&
            !structurallySend({true, true, true, false, false}) &&
            !structurallySync({true, true, false, false, true}),
        "Send/Sync derive structurally while raw pointers and negative impls fail closed");
    tests.expect(taskBorrowMayEscapeScope(true, true) && !taskBorrowMayEscapeScope(false, true),
        "task scopes require child join before borrowed state can leave the scope");
    tests.expect(validMemoryOrder(AtomicOperation::Load, MemoryOrder::Acquire) &&
            !validMemoryOrder(AtomicOperation::Load, MemoryOrder::Release) &&
            !validMemoryOrder(AtomicOperation::Store, MemoryOrder::Acquire) &&
            !volatileProvidesSynchronization(),
        "atomic orderings are operation-specific and volatile is never synchronization");

    const std::string manifest = concurrencyContractManifest();
    tests.expect(manifest.find("scheduler=explicit-job-system-only") != std::string::npos &&
            manifest.find("implicit-heap=no") != std::string::npos &&
            manifest.find("volatile-synchronization=no") != std::string::npos,
        "versioned concurrency policy forbids hidden scheduler, heap, and volatile synchronization");
    tests.expect(concurrencyContractFingerprintHex().size() == 16,
        "concurrency architecture has a stable policy fingerprint");
    const std::filesystem::path executable = argc > 0 ? std::filesystem::path(argv[0]) : std::filesystem::path{};
    const std::filesystem::path contract = executable.parent_path() / "t10-concurrency.contract";
    tests.expect(writeConcurrencyContract(contract, error) &&
            verifyConcurrencyContractFile(contract, error),
        "versioned concurrency contract round-trips through the compiler-owned format");
    std::error_code cleanupError;
    std::filesystem::remove(contract, cleanupError);
    return tests.result();
}
