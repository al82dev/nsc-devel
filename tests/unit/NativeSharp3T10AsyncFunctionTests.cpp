#include "nativesharp/Language/CoroutineSemantics.h"

#include "../support/TestContext.h"

#include <array>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;

    const std::array locals = {
        GeneratorLocal{8, 8, true, true, true, false, 1, 2},
        GeneratorLocal{4, 4, true, false, false, true, 2, 2},
        GeneratorLocal{32, 8, false, true, false, false, 0, 0}};
    const std::array awaits = {
        AwaitSuspension{1, 1, false, false, false},
        AwaitSuspension{2, 3, true, true, true}};
    std::string error;
    const auto plan = buildAsyncFunctionPlan(
        "builtin:int", locals, awaits, FrameStorage::CallerArena, true, error);

    tests.expect(plan.has_value() && error.empty() && plan->start == FutureStart::Lazy &&
            plan->storage == FrameStorage::CallerArena && !plan->completesSynchronously,
        "native async functions produce lazy futures in caller-selected storage");
    tests.expect(plan.has_value() && plan->stateMachine.states.size() == 2 &&
            plan->stateMachine.completedState == 3 &&
            plan->stateMachine.states[0].cleanupDepth == 1 &&
            plan->stateMachine.states[1].cleanupDepth == 3,
        "await points lower to contiguous states with exact defer/finally depth");
    tests.expect(plan.has_value() && plan->stateMachine.slots.size() == 2 &&
            plan->stateMachine.frame.size == 24 && plan->stateMachine.frame.alignment == 8 &&
            plan->stateMachine.frame.requiresPin,
        "captures and locals live across await determine one aligned pinnable frame");
    tests.expect(plan.has_value() && !plan->hasImplicitHeapAllocation &&
            plan->opaqueFutureIdentity.starts_with("future<builtin:int>#") &&
            plan->opaqueFutureIdentity.size() > 20,
        "the future type is opaque and stable without implicit heap allocation");

    const auto same = buildAsyncFunctionPlan(
        "builtin:int", locals, awaits, FrameStorage::CallerArena, true, error);
    const auto differentStorage = buildAsyncFunctionPlan(
        "builtin:int", locals, awaits, FrameStorage::ExplicitHeap, true, error);
    tests.expect(same.has_value() && differentStorage.has_value() &&
            same->opaqueFutureIdentity == plan->opaqueFutureIdentity &&
            differentStorage->opaqueFutureIdentity != plan->opaqueFutureIdentity,
        "opaque future identity is deterministic and includes the explicit storage ABI");

    const std::array badBorrow = {
        AwaitSuspension{1, 0, true, true, false}};
    tests.expect(!buildAsyncFunctionPlan(
            "builtin:int", std::span<const GeneratorLocal>{}, badBorrow,
            FrameStorage::CallerStack, false, error).has_value() &&
            error.find("stable address") != std::string::npos,
        "a borrow crosses await only with lifetime and address-stability proofs");

    const std::array badState = {
        AwaitSuspension{2, 0, false, false, false}};
    tests.expect(!buildAsyncFunctionPlan(
            "builtin:int", std::span<const GeneratorLocal>{}, badState,
            FrameStorage::CallerStack, false, error).has_value() &&
            error.find("contiguous") != std::string::npos,
        "await state numbering is deterministic and rejects gaps");

    const auto ready = buildAsyncFunctionPlan(
        "builtin:void", std::span<const GeneratorLocal>{}, std::span<const AwaitSuspension>{},
        FrameStorage::CallerStack, false, error);
    tests.expect(ready.has_value() && ready->completesSynchronously &&
            ready->stateMachine.states.empty() && ready->stateMachine.completedState == 1,
        "an async function without await returns an already-completable opaque future");

    const std::array impossibleLocal = {
        GeneratorLocal{4, 4, true, false, false, false, 1, 1}};
    tests.expect(!buildAsyncFunctionPlan(
            "builtin:void", impossibleLocal, std::span<const AwaitSuspension>{},
            FrameStorage::CallerStack, false, error).has_value() &&
            error.find("without await") != std::string::npos,
        "a synchronous-completing async function cannot claim suspended local liveness");

    return tests.result();
}
