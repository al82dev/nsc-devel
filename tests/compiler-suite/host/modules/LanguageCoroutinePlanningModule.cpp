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
}
}
