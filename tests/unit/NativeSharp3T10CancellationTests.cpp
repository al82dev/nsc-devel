#include "nativesharp/Language/CoroutineSemantics.h"

#include "../support/TestContext.h"

#include <array>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;

    const std::array locals = {
        GeneratorLocal{8, 8, true, true, true, false, 1, 2},
        GeneratorLocal{8, 8, true, true, false, false, 2, 2},
        GeneratorLocal{4, 4, true, false, false, false, 1, 2}};
    const std::array awaits = {
        AwaitSuspension{1, 1, false, false, false},
        AwaitSuspension{2, 3, false, false, false}};
    const auto function = buildAsyncFunctionPlan(
        "builtin:int", locals, awaits, FrameStorage::CallerArena, false, error);
    tests.expect(function.has_value(), "the cancellation fixture owns a valid suspended frame");

    const std::array<std::string, 3> names = {"socket", "buffer", "counter"};
    const std::array defers = {
        ActiveAsyncDefer{"trace", 1},
        ActiveAsyncDefer{"unlock", 3},
        ActiveAsyncDefer{"transaction", 2}};
    const auto cancellation = function.has_value()
        ? buildAsyncCancellationPlan(
            *function, 2, names, defers, CancellationReason::FutureDropped, error)
        : std::nullopt;
    tests.expect(cancellation.has_value() && cancellation->reason == CancellationReason::FutureDropped &&
            cancellation->cooperative && !cancellation->usesThreadAbort,
        "dropping a Future creates cooperative cancellation without thread abort");
    tests.expect(cancellation.has_value() && cancellation->actions.size() == 5 &&
            cancellation->actions[0].name == "unlock" &&
            cancellation->actions[1].name == "transaction" &&
            cancellation->actions[2].name == "trace",
        "active defer/finally actions run from deepest to shallowest before local drops");
    tests.expect(cancellation.has_value() &&
            cancellation->actions[3].kind == AsyncCleanupKind::LocalDrop &&
            cancellation->actions[3].name == "buffer" &&
            cancellation->actions[4].name == "socket",
        "only live drop-bearing frame slots run in reverse construction order");

    const std::array<ActiveAsyncDefer, 1> firstStateDefers = {
        ActiveAsyncDefer{"trace", 1}};
    const auto firstState = function.has_value()
        ? buildAsyncCancellationPlan(
            *function, 1, names, firstStateDefers, CancellationReason::Requested, error)
        : std::nullopt;
    tests.expect(firstState.has_value() && firstState->actions.size() == 2 &&
            firstState->actions[0].name == "trace" && firstState->actions[1].name == "socket",
        "cancelling earlier does not drop a local whose lifetime begins at a later await");

    tests.expect(function.has_value() && !buildAsyncCancellationPlan(
            *function, function->stateMachine.completedState, names, defers,
            CancellationReason::Requested, error).has_value() &&
            error.find("currently suspended") != std::string::npos,
        "a completed async function cannot be cancelled as though it were suspended");

    const std::array<std::string, 2> missingName = {"socket", "buffer"};
    tests.expect(function.has_value() && !buildAsyncCancellationPlan(
            *function, 2, missingName, defers,
            CancellationReason::Requested, error).has_value() &&
            error.find("one diagnostic name") != std::string::npos,
        "cleanup planning rejects incomplete frame-slot identity");

    const std::array invalidDefer = {ActiveAsyncDefer{"too-deep", 4}};
    tests.expect(function.has_value() && !buildAsyncCancellationPlan(
            *function, 2, names, invalidDefer,
            CancellationReason::ParentScopeExited, error).has_value() &&
            error.find("cleanup depth") != std::string::npos,
        "a defer outside the suspended cleanup stack cannot be executed accidentally");

    FutureState state;
    tests.expect(state.poll(PollOutcome::Pending, error) && state.cancel(error) &&
            state.state() == PollState::Cancelled && !state.consume(error),
        "Future terminal state agrees with the frame-destruction cancellation plan");
    return tests.result();
}
