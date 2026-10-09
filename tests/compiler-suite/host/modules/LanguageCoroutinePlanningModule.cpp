#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/CoroutineSemantics.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::language;

void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LANG-005", "Host.Language", "unit",
        "PR", {"language", "coroutine", "planning"}, std::move(function)});
}

void registerFutureStoragePolicyCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-050", [](TestContext& context) {
        context.expect(nativeFutureStart() == FutureStart::Lazy,
            "TL-LANG-050 native futures must always start lazily");
        for (const FrameStorage storage : std::array{
                 FrameStorage::CallerStack, FrameStorage::CallerArena, FrameStorage::ExplicitHeap})
            context.expect(!asyncFrameHasImplicitHeap(storage),
                "TL-LANG-050 no explicit frame storage may imply a hidden heap allocation");
    });
}

void registerBorrowAcrossAwaitCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-051", [](TestContext& context) {
        for (const bool outlives : std::array{false, true})
            for (const bool stable : std::array{false, true})
                context.expect(borrowMayCrossAwait(outlives, stable) == (outlives && stable),
                    "TL-LANG-051 borrow across await must require both independent proofs");
    });
}

void registerSynchronousAsyncPlanCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-052", [](TestContext& context) {
        std::string error = "stale";
        const std::vector<GeneratorLocal> noLocals;
        const std::vector<AwaitSuspension> noAwaits;
        const auto empty = buildAsyncFunctionPlan(
            "i32", noLocals, noAwaits, FrameStorage::CallerStack, false, error);
        context.expect(empty.has_value() && error.empty() && empty->completesSynchronously &&
                empty->returnType == "i32" && empty->start == FutureStart::Lazy &&
                empty->storage == FrameStorage::CallerStack &&
                empty->stateMachine.storage == GeneratorStorage::Stack &&
                empty->stateMachine.frame.size == 1 && empty->stateMachine.frame.alignment == 1 &&
                !empty->stateMachine.frame.requiresPin && empty->stateMachine.completedState == 1 &&
                !empty->hasImplicitHeapAllocation && empty->opaqueFutureIdentity.starts_with("future<i32>#"),
            "TL-LANG-052 an await-free empty function must produce the exact synchronous plan");
        const std::array inactive{GeneratorLocal{8, 8, false, true, true, true, 0, 0}};
        context.expect(buildAsyncFunctionPlan(
                "i32", inactive, noAwaits, FrameStorage::CallerStack, false, error).has_value() &&
                error.empty(),
            "TL-LANG-052 locals not live across suspension must not block a synchronous plan");
        const std::array active{GeneratorLocal{8, 8, true, true, false, false, 1, 1}};
        context.expect(!buildAsyncFunctionPlan(
                "i32", active, noAwaits, FrameStorage::CallerStack, false, error).has_value() &&
                error == "an async function without await cannot have locals live across suspension",
            "TL-LANG-052 a live local without await must use the exact error");
    });
}

void registerSuspendingAsyncPlanCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-053", [](TestContext& context) {
        const std::array locals{GeneratorLocal{8, 8, true, true, false, false, 1, 2}};
        const std::array valid{
            AwaitSuspension{1, 1, true, true, true}, AwaitSuspension{2, 2, false, false, false}};
        std::string error;
        const auto plan = buildAsyncFunctionPlan(
            "i32", locals, valid, FrameStorage::CallerArena, false, error);
        context.expect(plan.has_value() && error.empty() && !plan->completesSynchronously &&
                plan->stateMachine.states.size() == 2 && plan->stateMachine.slots.size() == 1 &&
                plan->stateMachine.storage == GeneratorStorage::Arena &&
                plan->stateMachine.states[0].state == 1 &&
                plan->stateMachine.states[1].state == 2,
            "TL-LANG-053 contiguous awaits and proven borrows must produce the exact state plan");
        const std::array hole{AwaitSuspension{2, 0, false, false, false}};
        context.expect(!buildAsyncFunctionPlan(
                "i32", {}, hole, FrameStorage::CallerStack, false, error).has_value() &&
                error == "async await states must be contiguous and start at one",
            "TL-LANG-053 a first-state hole must use the exact contiguity error");
        const std::array duplicate{
            AwaitSuspension{1, 0, false, false, false}, AwaitSuspension{1, 0, false, false, false}};
        context.expect(!buildAsyncFunctionPlan(
                "i32", {}, duplicate, FrameStorage::CallerStack, false, error).has_value() &&
                error == "async await states must be contiguous and start at one",
            "TL-LANG-053 a duplicate await state must use the exact contiguity error");
        for (const auto [outlives, stable] : std::array{
                 std::pair{false, false}, std::pair{false, true}, std::pair{true, false}})
        {
            const std::array invalid{AwaitSuspension{1, 0, true, outlives, stable}};
            context.expect(!buildAsyncFunctionPlan(
                    "i32", {}, invalid, FrameStorage::CallerStack, false, error).has_value() &&
                    error == "borrow across await requires source lifetime and stable address proofs",
                "TL-LANG-053 every missing borrow proof must use the exact error");
        }
    });
}

void registerAsyncPlanBoundaryCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-054", [](TestContext& context) {
        std::string error;
        const std::array oneAwait{AwaitSuspension{1, 0, false, false, false}};
        context.expect(!buildAsyncFunctionPlan(
                "", {}, oneAwait, FrameStorage::CallerStack, false, error).has_value() &&
                error == "an async function requires a resolved return type",
            "TL-LANG-054 an empty return type must have first rejection priority");
        const std::array mappings{
            std::pair{FrameStorage::CallerStack, GeneratorStorage::Stack},
            std::pair{FrameStorage::CallerArena, GeneratorStorage::Arena},
            std::pair{FrameStorage::ExplicitHeap, GeneratorStorage::ExplicitHeap}};
        for (const auto [storage, expected] : mappings)
        {
            const auto plan = buildAsyncFunctionPlan("R", {}, oneAwait, storage, true, error);
            context.expect(plan.has_value() && error.empty() && plan->storage == storage &&
                    plan->stateMachine.storage == expected && plan->stateMachine.frame.requiresPin &&
                    !plan->hasImplicitHeapAllocation,
                "TL-LANG-054 storage mapping and self-reference pinning must remain exact");
        }
    });
}

void registerFutureProtocolCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-055", [](TestContext& context) {
        FutureProtocol protocol{"Value", "Error", "Cancelled", true, true, true, true};
        std::string error = "stale";
        context.expect(validateFutureProtocol(protocol, error) && error.empty(),
            "TL-LANG-055 a complete static single-consumer protocol must succeed");
        protocol.valueType.clear();
        context.expect(!validateFutureProtocol(protocol, error) && error == "Future value type is unresolved",
            "TL-LANG-055 missing value type must have first rejection priority");
        protocol.valueType = "Value"; protocol.errorType.clear();
        context.expect(!validateFutureProtocol(protocol, error) &&
                error == "Future error and cancellation types must be explicit",
            "TL-LANG-055 missing error type must use the exact explicit-types error");
        protocol.errorType = "Error"; protocol.cancellationType.clear();
        context.expect(!validateFutureProtocol(protocol, error) &&
                error == "Future error and cancellation types must be explicit",
            "TL-LANG-055 missing cancellation type must use the exact explicit-types error");
        protocol.cancellationType = "Cancelled";
        for (std::size_t index = 0; index < 4; ++index)
        {
            protocol.hasPoll = protocol.hasResume = protocol.singleConsumer = protocol.staticDispatch = true;
            if (index == 0) protocol.hasPoll = false;
            if (index == 1) protocol.hasResume = false;
            if (index == 2) protocol.singleConsumer = false;
            if (index == 3) protocol.staticDispatch = false;
            context.expect(!validateFutureProtocol(protocol, error) &&
                    error == "Future requires static Poll/Resume and a single-consumer contract",
                "TL-LANG-055 every incomplete protocol capability must use the exact error");
        }
    });
}

void registerAwaitContextCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-056", [](TestContext& context) {
        std::string error = "stale";
        context.expect(awaitAllowed({true, false, false}, error) && error.empty(),
            "TL-LANG-056 an ordinary async body must allow await and clear stale errors");
        context.expect(!awaitAllowed({false, true, true}, error) &&
                error == "await is only valid inside an async function",
            "TL-LANG-056 non-async context must have first rejection priority");
        context.expect(!awaitAllowed({true, true, true}, error) &&
                error == "await is not valid during deterministic comptime execution",
            "TL-LANG-056 comptime must precede cleanup rejection");
        context.expect(!awaitAllowed({true, false, true}, error) &&
                error == "await cannot suspend inside a no-suspend cleanup region",
            "TL-LANG-056 no-suspend cleanup must use the exact error");
    });
}

enum class FutureFixture
{
    Created,
    Pending,
    ReadyValue,
    ReadyError,
    Cancelled,
    Consumed
};

FutureState makeFutureState(FutureFixture fixture)
{
    FutureState future;
    std::string error;
    switch (fixture)
    {
        case FutureFixture::Created: break;
        case FutureFixture::Pending: future.poll(PollOutcome::Pending, error); break;
        case FutureFixture::ReadyValue: future.poll(PollOutcome::ReadyValue, error); break;
        case FutureFixture::ReadyError: future.poll(PollOutcome::ReadyError, error); break;
        case FutureFixture::Cancelled: future.cancel(error); break;
        case FutureFixture::Consumed:
            future.poll(PollOutcome::ReadyValue, error);
            future.consume(error);
            break;
    }
    return future;
}

struct FutureFixtureExpectation final
{
    FutureFixture fixture;
    PollState state;
    FutureCompletion completion;
    bool resumable;
};

constexpr std::array kFutureFixtures{
    FutureFixtureExpectation{FutureFixture::Created, PollState::Created, FutureCompletion::None, true},
    FutureFixtureExpectation{FutureFixture::Pending, PollState::Pending, FutureCompletion::None, true},
    FutureFixtureExpectation{FutureFixture::ReadyValue, PollState::Ready, FutureCompletion::Value, false},
    FutureFixtureExpectation{FutureFixture::ReadyError, PollState::Ready, FutureCompletion::Error, false},
    FutureFixtureExpectation{FutureFixture::Cancelled, PollState::Cancelled, FutureCompletion::None, false},
    FutureFixtureExpectation{FutureFixture::Consumed, PollState::Consumed, FutureCompletion::Value, false}};

void registerFuturePollStateCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-057", [](TestContext& context) {
        const std::array outcomes{
            std::pair{PollOutcome::Pending, std::pair{PollState::Pending, FutureCompletion::None}},
            std::pair{PollOutcome::ReadyValue, std::pair{PollState::Ready, FutureCompletion::Value}},
            std::pair{PollOutcome::ReadyError, std::pair{PollState::Ready, FutureCompletion::Error}}};
        for (const FutureFixtureExpectation& initial : kFutureFixtures)
        {
            for (const auto& [outcome, expected] : outcomes)
            {
                FutureState future = makeFutureState(initial.fixture);
                std::string error = "stale";
                const bool accepted = future.poll(outcome, error);
                if (initial.resumable)
                {
                    context.expect(accepted && error.empty() && future.state() == expected.first &&
                            future.completion() == expected.second,
                        "TL-LANG-057 Created and Pending must accept every poll outcome exactly");
                }
                else
                {
                    context.expect(!accepted &&
                            error == "future cannot be resumed after completion, cancellation, or consumption" &&
                            future.state() == initial.state && future.completion() == initial.completion,
                        "TL-LANG-057 terminal states must reject polling without mutation");
                }
            }
        }
    });
}

void registerFutureConsumeStateCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-058", [](TestContext& context) {
        for (const FutureFixtureExpectation& initial : kFutureFixtures)
        {
            FutureState future = makeFutureState(initial.fixture);
            std::string error = "stale";
            const bool accepted = future.consume(error);
            const bool ready = initial.state == PollState::Ready;
            if (ready)
            {
                context.expect(accepted && error.empty() && future.state() == PollState::Consumed &&
                        future.completion() == initial.completion,
                    "TL-LANG-058 each Ready completion must be consumable exactly once");
                context.expect(!future.consume(error) && error == "future result is not ready" &&
                        future.state() == PollState::Consumed && future.completion() == initial.completion,
                    "TL-LANG-058 a consumed result must reject a second consume without mutation");
            }
            else
            {
                context.expect(!accepted && error == "future result is not ready" &&
                        future.state() == initial.state && future.completion() == initial.completion,
                    "TL-LANG-058 every non-Ready state must reject consume without mutation");
            }
        }
    });
}

void registerFutureCancelStateCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-059", [](TestContext& context) {
        for (const FutureFixtureExpectation& initial : kFutureFixtures)
        {
            FutureState future = makeFutureState(initial.fixture);
            std::string error = "stale";
            const bool accepted = future.cancel(error);
            const bool cancellable = initial.state == PollState::Created || initial.state == PollState::Pending;
            if (cancellable)
            {
                context.expect(accepted && error.empty() && future.state() == PollState::Cancelled &&
                        future.completion() == FutureCompletion::None,
                    "TL-LANG-059 Created and Pending must transition to Cancelled exactly");
            }
            else
            {
                context.expect(!accepted && error == "completed future cannot be cancelled" &&
                        future.state() == initial.state && future.completion() == initial.completion,
                    "TL-LANG-059 terminal states must reject cancellation without mutation");
            }
        }
    });
}

void registerCancellationCleanupOrderCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-060", [](TestContext& context) {
        const std::vector<std::string> none;
        const std::array locals{std::string("local0"), std::string("local1"), std::string("local2")};
        const std::array defers{std::string("defer0"), std::string("defer1")};
        context.expect(cancellationCleanup(none, none).empty(),
            "TL-LANG-060 empty locals and defers must produce empty cleanup");
        context.expect(cancellationCleanup(locals, none) ==
                std::vector<std::string>{"local2", "local1", "local0"},
            "TL-LANG-060 locals must be cleaned in reverse declaration order");
        context.expect(cancellationCleanup(none, defers) ==
                std::vector<std::string>{"defer1", "defer0"},
            "TL-LANG-060 defers must be cleaned in reverse registration order");
        context.expect(cancellationCleanup(locals, defers) ==
                std::vector<std::string>{"defer1", "defer0", "local2", "local1", "local0"},
            "TL-LANG-060 all defers must precede all locals in the exact contractual order");
    });
}

AsyncFunctionPlan makeCancellationFixture(TestContext& context)
{
    const std::array locals{
        GeneratorLocal{4, 4, true, true, false, false, 1, 3},
        GeneratorLocal{4, 4, true, true, false, false, 1, 1},
        GeneratorLocal{4, 4, true, false, false, false, 1, 3},
        GeneratorLocal{4, 4, true, true, false, false, 2, 3}};
    const std::array awaits{
        AwaitSuspension{1, 1, false, false, false},
        AwaitSuspension{2, 2, false, false, false},
        AwaitSuspension{3, 3, false, false, false}};
    std::string error;
    auto plan = buildAsyncFunctionPlan(
        "i32", locals, awaits, FrameStorage::CallerArena, false, error);
    context.expect(plan.has_value() && error.empty() && plan->stateMachine.slots.size() == 4,
        "CL-LANG-005 cancellation fixture must remain constructible");
    return plan.value_or(AsyncFunctionPlan{});
}

bool actionMatches(
    const AsyncCleanupAction& action,
    AsyncCleanupKind kind,
    std::string_view name,
    std::optional<std::size_t> frameSlot)
{
    return action.kind == kind && action.name == name && action.frameSlot == frameSlot;
}

void registerAsyncCancellationPlanCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-061", [](TestContext& context) {
        const AsyncFunctionPlan function = makeCancellationFixture(context);
        const std::array names{
            std::string("local0"), std::string("local1"), std::string("local2"), std::string("local3")};
        const std::array reasons{
            CancellationReason::Requested,
            CancellationReason::FutureDropped,
            CancellationReason::ParentScopeExited};
        for (std::size_t state = 1; state <= 3; ++state)
        {
            std::vector<ActiveAsyncDefer> defers{{"outer", 1}};
            if (state >= 2)
            {
                defers.push_back({"inner-a", 2});
                defers.push_back({"inner-b", 2});
            }
            if (state == 3) defers.push_back({"deepest", 3});
            for (const CancellationReason reason : reasons)
            {
                std::string error = "stale";
                const auto plan = buildAsyncCancellationPlan(
                    function, state, names, defers, reason, error);
                context.expect(plan.has_value() && error.empty() && plan->reason == reason &&
                        plan->suspendedState == state && plan->cooperative && !plan->usesThreadAbort,
                    "TL-LANG-061 every state and reason must preserve the cancellation contract");
                if (!plan.has_value()) continue;
                const std::size_t deferCount = state == 1 ? 1 : (state == 2 ? 3 : 4);
                context.expect(plan->actions.size() == deferCount + 2,
                    "TL-LANG-061 each state must contain all active defers and live droppable slots");
                if (plan->actions.size() != deferCount + 2) continue;
                if (state == 3)
                    context.expect(actionMatches(plan->actions[0], AsyncCleanupKind::Defer, "deepest", std::nullopt),
                        "TL-LANG-061 the deepest defer must run first");
                const std::size_t levelTwo = state == 3 ? 1 : 0;
                if (state >= 2)
                {
                    context.expect(
                        actionMatches(plan->actions[levelTwo], AsyncCleanupKind::Defer, "inner-a", std::nullopt) &&
                            actionMatches(
                                plan->actions[levelTwo + 1], AsyncCleanupKind::Defer, "inner-b", std::nullopt),
                        "TL-LANG-061 equal-depth defers must preserve stable registration order");
                }
                context.expect(actionMatches(
                        plan->actions[deferCount - 1], AsyncCleanupKind::Defer, "outer", std::nullopt),
                    "TL-LANG-061 the outer defer must be the final defer action");
                const std::size_t expectedFirstSlot = state == 1 ? 1 : 3;
                context.expect(actionMatches(plan->actions[deferCount], AsyncCleanupKind::LocalDrop,
                                   state == 1 ? "local1" : "local3", expectedFirstSlot) &&
                        actionMatches(plan->actions[deferCount + 1], AsyncCleanupKind::LocalDrop, "local0", 0),
                    "TL-LANG-061 live droppable slots must follow defers in reverse slot order");
            }
        }
    });
}

void registerInvalidAsyncCancellationPlanCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-062", [](TestContext& context) {
        const AsyncFunctionPlan function = makeCancellationFixture(context);
        const std::array names{
            std::string("local0"), std::string("local1"), std::string("local2"), std::string("local3")};
        const std::array shortNames{std::string("local0"), std::string("local1"), std::string("local2")};
        const std::array invalidStateDefer{ActiveAsyncDefer{"", 0}};
        std::string error = "stale";
        context.expect(!buildAsyncCancellationPlan(
                function, 99, shortNames, invalidStateDefer,
                CancellationReason::Requested, error).has_value() &&
                error == "cancellation requires a currently suspended async state",
            "TL-LANG-062 invalid suspended state must have first rejection priority");
        context.expect(!buildAsyncCancellationPlan(
                function, 1, shortNames, {}, CancellationReason::Requested, error).has_value() &&
                error == "async cancellation requires one diagnostic name per frame slot",
            "TL-LANG-062 mismatched slot names must reject without a partial plan");
        const std::array invalidDefers{
            ActiveAsyncDefer{"", 1}, ActiveAsyncDefer{"zero", 0}, ActiveAsyncDefer{"deep", 2}};
        for (const ActiveAsyncDefer& defer : invalidDefers)
        {
            const std::array active{defer};
            context.expect(!buildAsyncCancellationPlan(
                    function, 1, names, active, CancellationReason::Requested, error).has_value() &&
                    error == "active async defer must have a name and fit the suspended cleanup depth",
                "TL-LANG-062 each invalid defer must reject without a partial plan");
        }
    });
}
}

void registerLanguageCoroutinePlanningModule(TestRegistry& registry)
{
    registerFutureStoragePolicyCase(registry);
    registerBorrowAcrossAwaitCase(registry);
    registerSynchronousAsyncPlanCase(registry);
    registerSuspendingAsyncPlanCase(registry);
    registerAsyncPlanBoundaryCase(registry);
    registerFutureProtocolCase(registry);
    registerAwaitContextCase(registry);
    registerFuturePollStateCase(registry);
    registerFutureConsumeStateCase(registry);
    registerFutureCancelStateCase(registry);
    registerCancellationCleanupOrderCase(registry);
    registerAsyncCancellationPlanCase(registry);
    registerInvalidAsyncCancellationPlanCase(registry);
}
}
