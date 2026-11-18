#include "nativesharp/Language/CoroutineSemantics.h"

#include "../support/TestContext.h"

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;

    const FutureProtocol typed{
        "builtin:int", "Errors.IoError", "Tasks.Cancelled", "", true, true, true, true};
    tests.expect(validateFutureProtocol(typed, error) && error.empty(),
        "Future publishes explicit value, error and cancellation types");

    FutureProtocol missingError = typed;
    missingError.errorType.clear();
    tests.expect(!validateFutureProtocol(missingError, error) &&
            error.find("explicit") != std::string::npos,
        "an untyped error or cancellation path is rejected");

    FutureProtocol dynamicPoll = typed;
    dynamicPoll.staticDispatch = false;
    tests.expect(!validateFutureProtocol(dynamicPoll, error) &&
            error.find("static") != std::string::npos,
        "await uses the static Poll/Resume protocol without hidden virtual dispatch");

    tests.expect(awaitAllowed({true, false, false}, error),
        "await is accepted in a suspendable async function body");
    tests.expect(!awaitAllowed({false, false, false}, error) &&
            error.find("async function") != std::string::npos,
        "await outside async is rejected at its source context");
    tests.expect(!awaitAllowed({true, true, false}, error) &&
            error.find("comptime") != std::string::npos &&
            !awaitAllowed({true, false, true}, error) &&
            error.find("cleanup") != std::string::npos,
        "await cannot enter deterministic comptime or a no-suspend cleanup region");

    FutureState value;
    tests.expect(value.poll(PollOutcome::Pending, error) &&
            value.poll(PollOutcome::ReadyValue, error) &&
            value.state() == PollState::Ready &&
            value.completion() == FutureCompletion::Value && value.consume(error),
        "Poll Pending/ReadyValue produces one consumable typed value");

    FutureState failure;
    tests.expect(failure.poll(PollOutcome::ReadyError, error) &&
            failure.completion() == FutureCompletion::Error && failure.consume(error),
        "Poll ReadyError preserves the typed error branch until consumption");

    FutureState cancelled;
    tests.expect(cancelled.poll(PollOutcome::Pending, error) && cancelled.cancel(error) &&
            cancelled.state() == PollState::Cancelled &&
            cancelled.completion() == FutureCompletion::None &&
            !cancelled.poll(PollOutcome::ReadyValue, error),
        "cancelled futures cannot resume and expose no stale completion payload");

    return tests.result();
}
