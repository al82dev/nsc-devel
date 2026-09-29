#include "nativesharp/Language/CoroutineSemantics.h"

#include "../support/TestContext.h"

#include <array>

int main()
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
    return tests.result();
}
