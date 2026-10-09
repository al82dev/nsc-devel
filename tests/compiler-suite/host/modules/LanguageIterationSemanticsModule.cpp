#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/IterationSemantics.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
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
    registry.add(TestCase{std::move(id), "CL-LANG-011", "Host.Language", "unit", "PR",
        {"language", "iteration-semantics"}, std::move(function)});
}

std::array<GeneratorLocal, 4> validGeneratorLocals()
{
    return {
        GeneratorLocal{4, 4, true, false, false, false, 1, 3},
        GeneratorLocal{8, 8, true, true, true, false, 1, 3},
        GeneratorLocal{16, 8, true, true, false, true, 2, 2},
        GeneratorLocal{64, 8, false, true, false, false, 0, 0},
    };
}

std::array<GeneratorSuspension, 3> validGeneratorSuspensions()
{
    return {
        GeneratorSuspension{1, "builtin:int", GeneratorYieldKind::Value, 1, false},
        GeneratorSuspension{2, "builtin:int", GeneratorYieldKind::SharedReference, 3, true},
        GeneratorSuspension{3, "builtin:int", GeneratorYieldKind::Value, 2, false},
    };
}

std::optional<SynchronousGeneratorPlan> validGeneratorPlan(std::string& error)
{
    const auto locals = validGeneratorLocals();
    const auto suspensions = validGeneratorSuspensions();
    return buildSynchronousGeneratorPlan(
        locals, suspensions, GeneratorStorage::CallerProvided, false, error);
}

void registerIteratorProtocolCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-142", [](TestContext& context) {
        constexpr std::string_view required =
            "foreach requires item type, MoveNext, Current, and cleanup";
        const IteratorProtocol valid{
            "builtin:int", IterationMode::Value, true, true, true, false};
        std::string error = "stale";
        context.expect(validateIteratorProtocol(valid, error) && error.empty(),
            "TL-LANG-142 a complete iterator protocol must be accepted and clear stale errors");

        for (std::size_t mutation = 0; mutation < 4; ++mutation)
        {
            IteratorProtocol invalid = valid;
            if (mutation == 0) invalid.itemType.clear();
            if (mutation == 1) invalid.hasMoveNext = false;
            if (mutation == 2) invalid.hasCurrent = false;
            if (mutation == 3) invalid.hasCleanup = false;
            context.expect(!validateIteratorProtocol(invalid, error) && error == required,
                "TL-LANG-142 each required protocol field must return the exact shared error");
        }
    });
}

void registerLendingIteratorCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-143", [](TestContext& context) {
        constexpr std::string_view lendingError = "a lending iterator must yield by reference";
        IteratorProtocol protocol{
            "builtin:int", IterationMode::Value, true, true, true, true};
        std::string error;
        context.expect(!validateIteratorProtocol(protocol, error) && error == lendingError,
            "TL-LANG-143 a lending value iterator must be rejected exactly");

        protocol.mode = IterationMode::Reference;
        context.expect(validateIteratorProtocol(protocol, error) && error.empty(),
            "TL-LANG-143 a shared-reference lending iterator must be accepted");
        protocol.mode = IterationMode::MutableReference;
        context.expect(validateIteratorProtocol(protocol, error) && error.empty(),
            "TL-LANG-143 a mutable-reference lending iterator must be accepted");
    });
}

void registerLazyRangeCountCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-144", [](TestContext& context) {
        std::string error = "stale";
        context.expect(lazyRangeCount({0, 10, 2, false}, error) == 5 && error.empty(),
            "TL-LANG-144 an ascending exclusive range must have the exact count");
        context.expect(lazyRangeCount({0, 10, 2, true}, error) == 6 && error.empty(),
            "TL-LANG-144 an ascending inclusive range must include an aligned endpoint");
        context.expect(lazyRangeCount({10, 0, -2, false}, error) == 5 && error.empty(),
            "TL-LANG-144 a descending exclusive range must have the exact count");
        context.expect(lazyRangeCount({10, 0, -2, true}, error) == 6 && error.empty(),
            "TL-LANG-144 a descending inclusive range must include an aligned endpoint");
        context.expect(lazyRangeCount({0, 10, 3, true}, error) == 4 &&
                lazyRangeCount({10, 0, -3, true}, error) == 4,
            "TL-LANG-144 an unaligned endpoint must not create an extra element");
        context.expect(lazyRangeCount({7, 7, 1, false}, error) == 0 &&
                lazyRangeCount({7, 7, -1, true}, error) == 1,
            "TL-LANG-144 equal endpoints must distinguish empty and inclusive ranges");
    });
}

void registerLazyRangeFailureCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-145", [](TestContext& context) {
        std::string error;
        context.expect(!lazyRangeCount({0, 10, 0, false}, error).has_value() &&
                error == "range step cannot be zero",
            "TL-LANG-145 a zero step must return nullopt and its exact error");
        context.expect(!lazyRangeCount({0, 10, -1, false}, error).has_value() &&
                error == "range step moves away from its end",
            "TL-LANG-145 a descending step toward a greater endpoint must fail exactly");
        context.expect(!lazyRangeCount({10, 0, 1, false}, error).has_value() &&
                error == "range step moves away from its end",
            "TL-LANG-145 an ascending step toward a smaller endpoint must fail exactly");
        context.expect(!lazyRangeCount({
                (std::numeric_limits<std::int64_t>::min)(),
                (std::numeric_limits<std::int64_t>::max)(), 1, true}, error).has_value() &&
                error == "range element count overflows size_t",
            "TL-LANG-145 an inclusive full-width range must reject count overflow");
    });
}

void registerRangeAllocationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-146", [](TestContext& context) {
        context.expect(!rangeMaterializesCollection(),
            "TL-LANG-146 a range must remain a lazy value without collection materialization");
        constexpr std::array storages = {
            BuilderStorage::Stack, BuilderStorage::Caller, BuilderStorage::HeapExplicit};
        for (const BuilderStorage storage : storages)
        {
            context.expect(!comprehensionHasImplicitAllocation(storage),
                "TL-LANG-146 every explicit builder storage must avoid implicit allocation");
        }
    });
}

void registerBorrowedMutationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-147", [](TestContext& context) {
        IteratorProtocol protocol{
            "builtin:int", IterationMode::Value, true, true, true, false};
        context.expect(mutationAllowedDuringBorrowedIteration(protocol, SourceMutation::Element) &&
                mutationAllowedDuringBorrowedIteration(protocol, SourceMutation::Structural),
            "TL-LANG-147 a nonlending iterator must allow both mutation kinds");

        for (const IterationMode mode :
            {IterationMode::Reference, IterationMode::MutableReference})
        {
            protocol.mode = mode;
            protocol.lendsFromSource = true;
            context.expect(
                mutationAllowedDuringBorrowedIteration(protocol, SourceMutation::Element) &&
                    !mutationAllowedDuringBorrowedIteration(protocol, SourceMutation::Structural),
                "TL-LANG-147 lending must allow element mutation and reject structural mutation");
        }
    });
}

void registerLegacyGeneratorFrameCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-148", [](TestContext& context) {
        const std::array locals = {
            GeneratorLocal{4, 4, true},
            GeneratorLocal{64, 8, false},
            GeneratorLocal{8, 8, true},
        };
        const auto frame = computeGeneratorFrame(locals, true);
        context.expect(frame.has_value() && frame->size == 16 && frame->alignment == 8 &&
                frame->requiresPin,
            "TL-LANG-148 the legacy frame must store only live locals with exact layout and pinning");
        const auto empty = computeGeneratorFrame(std::span<const GeneratorLocal>{}, false);
        context.expect(empty.has_value() && empty->size == 1 && empty->alignment == 1 &&
                !empty->requiresPin,
            "TL-LANG-148 an empty frame must retain only the one-byte state discriminator");

        for (const GeneratorLocal invalid : {
            GeneratorLocal{0, 1, true}, GeneratorLocal{1, 0, true},
            GeneratorLocal{1, 3, true}, GeneratorLocal{1, 32, true}})
        {
            const std::array candidate = {invalid};
            context.expect(!computeGeneratorFrame(candidate, false).has_value(),
                "TL-LANG-148 invalid live-local size or alignment must return nullopt");
        }
    });
}

void registerGeneratorPlanCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-149", [](TestContext& context) {
        const auto locals = validGeneratorLocals();
        const auto suspensions = validGeneratorSuspensions();
        constexpr std::array storages = {GeneratorStorage::Stack, GeneratorStorage::CallerProvided,
            GeneratorStorage::Arena, GeneratorStorage::ExplicitHeap};
        for (const GeneratorStorage storage : storages)
        {
            std::string error = "stale";
            const auto plan = buildSynchronousGeneratorPlan(locals, suspensions, storage, false, error);
            context.expect(plan.has_value() && error.empty() && plan->storage == storage &&
                    plan->itemType == "builtin:int" && plan->completedState == 4 &&
                    plan->yieldsBorrowedItems && !plan->hasImplicitHeapAllocation,
                "TL-LANG-149 valid plans must preserve explicit storage, item, states, and allocation");
            if (!plan.has_value()) continue;
            context.expect(plan->frame.size == 32 && plan->frame.alignment == 8 &&
                    plan->frame.requiresPin && plan->slots.size() == 3 && plan->states.size() == 3,
                "TL-LANG-149 the full frame and state counts must be exact");
            context.expect(plan->slots[0].localIndex == 0 && plan->slots[0].offset == 4 &&
                    plan->slots[1].localIndex == 1 && plan->slots[1].offset == 8 &&
                    plan->slots[2].localIndex == 2 && plan->slots[2].offset == 16,
                "TL-LANG-149 frame slots must preserve local identity and aligned offsets");
            context.expect(plan->states[0].liveSlots == std::vector<std::size_t>({0, 1}) &&
                    plan->states[1].liveSlots == std::vector<std::size_t>({0, 1, 2}) &&
                    plan->states[2].liveSlots == std::vector<std::size_t>({0, 1}) &&
                    plan->states[0].resumeState == 2 && plan->states[1].resumeState == 3 &&
                    plan->states[2].resumeState == 4 && plan->states[1].cleanupDepth == 3,
                "TL-LANG-149 each suspension must expose exact liveness, resume, and cleanup state");
        }

        auto mutableBorrow = suspensions;
        mutableBorrow[1].kind = GeneratorYieldKind::MutableReference;
        std::string error;
        const auto mutablePlan = buildSynchronousGeneratorPlan(
            locals, mutableBorrow, GeneratorStorage::Arena, false, error);
        context.expect(mutablePlan.has_value() && mutablePlan->yieldsBorrowedItems &&
                mutablePlan->frame.requiresPin && error.empty(),
            "TL-LANG-149 a tied mutable-reference yield must pin the selected frame");

        const std::array valueYield = {
            GeneratorSuspension{1, "builtin:int", GeneratorYieldKind::Value, 0, false}};
        const auto selfReferential = buildSynchronousGeneratorPlan(
            std::span<const GeneratorLocal>{}, valueYield, GeneratorStorage::Stack, true, error);
        const auto ordinary = buildSynchronousGeneratorPlan(
            std::span<const GeneratorLocal>{}, valueYield, GeneratorStorage::Stack, false, error);
        context.expect(selfReferential.has_value() && selfReferential->frame.requiresPin &&
                ordinary.has_value() && !ordinary->frame.requiresPin,
            "TL-LANG-149 self-reference alone must control pinning for value-only generators");
    });
}

void registerGeneratorPlanFailureCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-150", [](TestContext& context) {
        const auto locals = validGeneratorLocals();
        const auto validSuspensions = validGeneratorSuspensions();
        std::string error;
        context.expect(!buildSynchronousGeneratorPlan(
                locals, std::span<const GeneratorSuspension>{}, GeneratorStorage::Stack,
                false, error).has_value() &&
                error == "a synchronous generator requires at least one productive yield",
            "TL-LANG-150 a generator without yields must fail exactly");

        auto invalidSuspensions = validSuspensions;
        invalidSuspensions[0].itemType.clear();
        context.expect(!buildSynchronousGeneratorPlan(
                locals, invalidSuspensions, GeneratorStorage::Stack, false, error).has_value() &&
                error == "a generator yield must have a resolved item type",
            "TL-LANG-150 an unresolved first item type must fail exactly");
        invalidSuspensions = validSuspensions;
        invalidSuspensions[1].itemType = "builtin:long";
        context.expect(!buildSynchronousGeneratorPlan(
                locals, invalidSuspensions, GeneratorStorage::Stack, false, error).has_value() &&
                error == "all generator yields must resolve to one item type",
            "TL-LANG-150 conflicting item types must fail exactly");
        invalidSuspensions = validSuspensions;
        invalidSuspensions[1].state = 3;
        context.expect(!buildSynchronousGeneratorPlan(
                locals, invalidSuspensions, GeneratorStorage::Stack, false, error).has_value() &&
                error == "generator suspension states must be contiguous and start at one",
            "TL-LANG-150 a suspension gap must fail exactly");
        for (const GeneratorYieldKind kind :
            {GeneratorYieldKind::SharedReference, GeneratorYieldKind::MutableReference})
        {
            invalidSuspensions = validSuspensions;
            invalidSuspensions[1].kind = kind;
            invalidSuspensions[1].lifetimeTiedToFrame = false;
            context.expect(!buildSynchronousGeneratorPlan(
                    locals, invalidSuspensions, GeneratorStorage::Stack, false, error).has_value() &&
                    error == "a borrowed generator yield must be tied to frame-owned or captured storage",
                "TL-LANG-150 either untied borrowed yield kind must fail exactly");
        }

        const std::array oneYield = {
            GeneratorSuspension{1, "builtin:int", GeneratorYieldKind::Value, 0, false}};
        for (const GeneratorLocal invalid : {
            GeneratorLocal{0, 1, true, false, false, false, 1, 1},
            GeneratorLocal{1, 0, true, false, false, false, 1, 1},
            GeneratorLocal{1, 3, true, false, false, false, 1, 1},
            GeneratorLocal{1, 32, true, false, false, false, 1, 1}})
        {
            const std::array invalidLocals = {invalid};
            context.expect(!buildSynchronousGeneratorPlan(
                    invalidLocals, oneYield, GeneratorStorage::Stack, false, error).has_value() &&
                    error == "generator local has an invalid size or alignment",
                "TL-LANG-150 each invalid local size or alignment must return the exact error");
        }
        for (const GeneratorLocal invalid : {
            GeneratorLocal{1, 1, true, false, false, false, 0, 1},
            GeneratorLocal{1, 1, true, false, false, false, 1, 0},
            GeneratorLocal{1, 1, true, false, false, false, 1, 2}})
        {
            const std::array invalidLocals = {invalid};
            context.expect(!buildSynchronousGeneratorPlan(
                    invalidLocals, oneYield, GeneratorStorage::Stack, false, error).has_value() &&
                    error == "generator local liveness must name a valid inclusive suspension interval",
                "TL-LANG-150 each invalid liveness interval must return the exact error");
        }
        const std::array overflowing = {GeneratorLocal{
            (std::numeric_limits<std::size_t>::max)(), 8, true, false, false, false, 1, 1}};
        context.expect(!buildSynchronousGeneratorPlan(
                overflowing, oneYield, GeneratorStorage::Stack, false, error).has_value() &&
                error == "generator frame layout overflows size_t",
            "TL-LANG-150 a frame-layout overflow must fail before native wraparound");
    });
}

void registerGeneratorCancellationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-151", [](TestContext& context) {
        std::string error;
        const auto plan = validGeneratorPlan(error);
        context.expect(plan.has_value() && error.empty(),
            "TL-LANG-151 the cancellation fixture must produce a valid plan");
        if (!plan.has_value()) return;
        context.expect(generatorCancellationDropOrder(*plan, 1) ==
                std::vector<std::size_t>({1}),
            "TL-LANG-151 state one must drop only its live droppable slot");
        context.expect(generatorCancellationDropOrder(*plan, 2) ==
                std::vector<std::size_t>({2, 1}),
            "TL-LANG-151 state two must drop live slots in reverse construction order");
        context.expect(generatorCancellationDropOrder(*plan, 3) ==
                std::vector<std::size_t>({1}),
            "TL-LANG-151 state three must exclude locals whose interval ended earlier");
        context.expect(generatorCancellationDropOrder(*plan, 0).empty() &&
                generatorCancellationDropOrder(*plan, plan->completedState).empty() &&
                generatorCancellationDropOrder(*plan, 99).empty(),
            "TL-LANG-151 entry, completed, and invalid states must have no cancellation drops");
    });
}

void registerGeneratorResumeCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-152", [](TestContext& context) {
        std::string error;
        const auto plan = validGeneratorPlan(error);
        context.expect(plan.has_value() && error.empty(),
            "TL-LANG-152 the resume fixture must produce a valid plan");
        if (!plan.has_value()) return;
        context.expect(generatorResumeState(*plan, 0) == 1 &&
                generatorResumeState(*plan, 1) == 2 && generatorResumeState(*plan, 2) == 3 &&
                generatorResumeState(*plan, 3) == 4,
            "TL-LANG-152 entry and each yield must advance to the exact next state");
        context.expect(!generatorResumeState(*plan, 4).has_value() &&
                !generatorResumeState(*plan, 99).has_value(),
            "TL-LANG-152 completed and invalid states must not resume");
        const SynchronousGeneratorPlan empty;
        context.expect(!generatorResumeState(empty, 0).has_value(),
            "TL-LANG-152 an empty state machine must not synthesize a first state");
    });
}

void registerFusionAnalysisCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-153", [](TestContext& context) {
        const EvaluationContract single{TraversalCategory::SinglePass, true, true};
        const EvaluationContract multi{TraversalCategory::MultiPass, true, true};
        const FusionDecision ordinary = analyzeFusion(FusionRequest{single, single});
        const FusionDecision reusable = analyzeFusion(FusionRequest{multi, multi});
        context.expect(ordinary.allowed && ordinary.rejection == FusionRejection::None &&
                ordinary.resultCategory == TraversalCategory::SinglePass &&
                reusable.allowed && reusable.resultCategory == TraversalCategory::MultiPass &&
                mayFuse(single, single) && mayFuse(multi, multi),
            "TL-LANG-153 valid fusion must preserve exact traversal-category semantics");

        FusionRequest request{single, multi};
        request.source.lazy = false;
        request.source.preservesLeftToRightEffects = false;
        request.adapter.repeatsElements = true;
        request.adapter.mayShortCircuit = true;
        request.source.yieldsBorrowedItems = true;
        request.adapter.buffersElements = true;
        request.fusedEvaluatesEachSourceAtMostOnce = false;
        request.fusedPreservesShortCircuit = false;
        context.expect(analyzeFusion(request).rejection == FusionRejection::EagerBoundary,
            "TL-LANG-153 eager boundary must have first rejection priority");

        request.source.lazy = true;
        context.expect(analyzeFusion(request).rejection == FusionRejection::EffectReordering,
            "TL-LANG-153 effect reordering must have second rejection priority");
        request.source.preservesLeftToRightEffects = true;
        context.expect(analyzeFusion(request).rejection == FusionRejection::TraversalUpgrade,
            "TL-LANG-153 traversal upgrade must have third rejection priority");
        request.adapter.category = TraversalCategory::SinglePass;
        context.expect(analyzeFusion(request).rejection == FusionRejection::RepeatedEvaluation,
            "TL-LANG-153 repeated evaluation must have fourth rejection priority");
        request.fusedEvaluatesEachSourceAtMostOnce = true;
        context.expect(analyzeFusion(request).rejection == FusionRejection::ShortCircuitLost,
            "TL-LANG-153 lost short-circuit must have fifth rejection priority");
        request.fusedPreservesShortCircuit = true;
        context.expect(analyzeFusion(request).rejection == FusionRejection::BorrowedItemBuffered,
            "TL-LANG-153 borrowed buffering must have sixth rejection priority");

        request.source.yieldsBorrowedItems = false;
        request.adapter.buffersElements = false;
        context.expect(analyzeFusion(request).allowed,
            "TL-LANG-153 removing the final rejection cause must permit fusion");
    });
}
}

void registerLanguageIterationSemanticsModule(TestRegistry& registry)
{
    registerIteratorProtocolCase(registry);
    registerLendingIteratorCase(registry);
    registerLazyRangeCountCase(registry);
    registerLazyRangeFailureCase(registry);
    registerRangeAllocationCase(registry);
    registerBorrowedMutationCase(registry);
    registerLegacyGeneratorFrameCase(registry);
    registerGeneratorPlanCase(registry);
    registerGeneratorPlanFailureCase(registry);
    registerGeneratorCancellationCase(registry);
    registerGeneratorResumeCase(registry);
    registerFusionAnalysisCase(registry);
}
}
