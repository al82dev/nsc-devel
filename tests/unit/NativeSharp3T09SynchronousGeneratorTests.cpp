#include "nativesharp/Language/IterationSemantics.h"

#include "../support/TestContext.h"

#include <array>
#include <limits>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;

    const std::array locals = {
        GeneratorLocal{4, 4, true, false, false, false, 1, 3},
        GeneratorLocal{8, 8, true, true, true, false, 1, 3},
        GeneratorLocal{16, 8, true, true, false, true, 2, 2},
        GeneratorLocal{64, 8, false, true, false, false, 0, 0}};
    const std::array suspensions = {
        GeneratorSuspension{1, "builtin:int", GeneratorYieldKind::Value, 1, false},
        GeneratorSuspension{2, "builtin:int", GeneratorYieldKind::SharedReference, 3, true},
        GeneratorSuspension{3, "builtin:int", GeneratorYieldKind::Value, 2, false}};
    std::string error;
    const auto plan = buildSynchronousGeneratorPlan(
        locals, suspensions, GeneratorStorage::CallerProvided, false, error);

    tests.expect(plan.has_value() && error.empty() && plan->states.size() == 3 &&
            plan->completedState == 4 && plan->itemType == "builtin:int",
        "multiple yields become contiguous native suspension and completion states");
    tests.expect(plan.has_value() && plan->slots.size() == 3 && plan->frame.size == 32 &&
            plan->frame.alignment == 8,
        "only live-across-yield locals occupy aligned frame slots");
    tests.expect(plan.has_value() && plan->states[0].liveSlots.size() == 2 &&
            plan->states[1].liveSlots.size() == 3 && plan->states[2].liveSlots.size() == 2 &&
            plan->states[1].cleanupDepth == 3,
        "each suspension freezes exact local liveness and defer/finally cleanup depth");
    tests.expect(plan.has_value() && plan->yieldsBorrowedItems && plan->frame.requiresPin &&
            !plan->hasImplicitHeapAllocation && plan->storage == GeneratorStorage::CallerProvided,
        "borrowed yields pin the caller-selected frame without implicit heap allocation");

    const std::vector<std::size_t> stateTwoDrops =
        plan.has_value() ? generatorCancellationDropOrder(*plan, 2) : std::vector<std::size_t>{};
    const std::vector<std::size_t> stateOneDrops =
        plan.has_value() ? generatorCancellationDropOrder(*plan, 1) : std::vector<std::size_t>{};
    tests.expect(stateTwoDrops == std::vector<std::size_t>({2, 1}) &&
            stateOneDrops == std::vector<std::size_t>({1}),
        "cancellation drops only active move-only/RAII slots in reverse construction order");
    tests.expect(plan.has_value() && generatorResumeState(*plan, 0) == 1 &&
            generatorResumeState(*plan, 1) == 2 && generatorResumeState(*plan, 2) == 3 &&
            generatorResumeState(*plan, 3) == 4 &&
            !generatorResumeState(*plan, 4).has_value(),
        "resume transitions are deterministic and completed generators cannot resume");

    const std::array badTypes = {
        GeneratorSuspension{1, "builtin:int", GeneratorYieldKind::Value, 0, false},
        GeneratorSuspension{2, "builtin:long", GeneratorYieldKind::Value, 0, false}};
    tests.expect(!buildSynchronousGeneratorPlan(
            locals, badTypes, GeneratorStorage::Stack, false, error).has_value() &&
            error.find("one item type") != std::string::npos,
        "inconsistent yield item types are rejected before HIR lowering");

    const std::array badBorrow = {
        GeneratorSuspension{1, "builtin:int", GeneratorYieldKind::MutableReference, 0, false}};
    tests.expect(!buildSynchronousGeneratorPlan(
            std::span<const GeneratorLocal>{}, badBorrow,
            GeneratorStorage::Arena, false, error).has_value() &&
            error.find("tied to frame") != std::string::npos,
        "a borrowed yield cannot escape storage absent from the generator frame");

    const std::array badStates = {
        GeneratorSuspension{2, "builtin:int", GeneratorYieldKind::Value, 0, false}};
    tests.expect(!buildSynchronousGeneratorPlan(
            std::span<const GeneratorLocal>{}, badStates,
            GeneratorStorage::ExplicitHeap, false, error).has_value() &&
            error.find("contiguous") != std::string::npos,
        "non-contiguous suspension numbering is rejected deterministically");

    const std::array overflowing = {
        GeneratorLocal{std::numeric_limits<std::size_t>::max(), 8, true,
            false, false, false, 1, 1}};
    const std::array oneYield = {
        GeneratorSuspension{1, "builtin:int", GeneratorYieldKind::Value, 0, false}};
    tests.expect(!buildSynchronousGeneratorPlan(
            overflowing, oneYield, GeneratorStorage::Stack, false, error).has_value() &&
            error.find("overflows") != std::string::npos,
        "frame size overflow is diagnosed instead of wrapping the native layout");

    return tests.result();
}
