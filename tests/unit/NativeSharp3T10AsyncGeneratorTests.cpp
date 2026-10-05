#include "nativesharp/Language/CoroutineSemantics.h"

#include "../support/TestContext.h"

#include <array>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;

    const std::array locals = {
        GeneratorLocal{16, 8, true, true, true, false, 1, 3},
        GeneratorLocal{8, 8, true, true, false, false, 2, 3}};
    const std::array suspensions = {
        AsyncGeneratorSuspension{1, AsyncGeneratorSuspensionKind::Await, 1, false, false, false},
        AsyncGeneratorSuspension{2, AsyncGeneratorSuspensionKind::Yield, 2, true, true, true},
        AsyncGeneratorSuspension{3, AsyncGeneratorSuspensionKind::Await, 2, false, false, false}};
    const auto plan = buildAsyncGeneratorPlan(
        "Packet", "IoError", "Cancelled", locals, suspensions,
        FrameStorage::CallerArena, true, true, error);
    tests.expect(plan.has_value() && plan->explicitBackpressure && plan->moveOnlyItems,
        "async generators publish explicit demand and preserve move-only item identity");
    tests.expect(plan.has_value() && plan->stateMachine.states.size() == 3 &&
            plan->stateMachine.yieldsBorrowedItems,
        "await and borrowed yield share one contiguous state machine");
    tests.expect(plan.has_value() && plan->stateMachine.frame.size >= 32 &&
            plan->stateMachine.frame.requiresPin,
        "the unified frame has a deterministic size and records address-sensitive borrows");
    tests.expect(plan.has_value() && plan->errorType == "IoError" &&
            plan->cancellationType == "Cancelled",
        "errors and cancellation remain distinct typed protocol outcomes");

    AsyncGeneratorProtocolState protocol;
    tests.expect(protocol.requestNext(error) &&
            protocol.resume(AsyncGeneratorPollOutcome::Pending, error) &&
            protocol.demandOutstanding() &&
            protocol.resume(AsyncGeneratorPollOutcome::ItemReady, error) &&
            protocol.itemOutstanding(),
        "ready and pending may alternate while preserving a single outstanding demand");
    tests.expect(!protocol.requestNext(error) && error.find("one outstanding") != std::string::npos &&
            protocol.takeItem(error) && protocol.requestNext(error),
        "backpressure forbids requesting another item before consuming the yielded item");
    tests.expect(protocol.stopEarly(error) && protocol.state() == AsyncGeneratorState::Cancelled &&
            !protocol.requestNext(error),
        "early stop produces typed cooperative cancellation and releases demand");

    AsyncGeneratorProtocolState failure;
    tests.expect(failure.requestNext(error) &&
            failure.resume(AsyncGeneratorPollOutcome::ReadyError, error) &&
            failure.state() == AsyncGeneratorState::Error && !failure.requestNext(error),
        "typed generator errors are terminal and cannot be mistaken for completion");

    auto invalidBorrow = suspensions;
    invalidBorrow[1].borrowAddressStable = false;
    tests.expect(!buildAsyncGeneratorPlan(
            "Packet", "IoError", "Cancelled", locals, invalidBorrow,
            FrameStorage::CallerArena, true, true, error).has_value() &&
            error.find("stable address") != std::string::npos,
        "borrowed items require lifetime and address-stability proofs");
    return tests.result();
}
