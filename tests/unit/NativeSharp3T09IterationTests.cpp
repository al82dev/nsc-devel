#include "nativesharp/Language/IterationSemantics.h"

#include "../support/TestContext.h"

#include <array>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;
    const IteratorProtocol values{"builtin:int", IterationMode::Value, true, true, true, false};
    const IteratorProtocol lending{"builtin:int", IterationMode::Reference, true, true, true, true};
    tests.expect(validateIteratorProtocol(values, error) && validateIteratorProtocol(lending, error),
        "foreach protocol supports owned and explicitly lending iterators");
    IteratorProtocol badLending = lending;
    badLending.mode = IterationMode::Value;
    tests.expect(!validateIteratorProtocol(badLending, error),
        "lending iteration cannot disguise a borrowed item as a value");

    tests.expect(lazyRangeCount({0, 10, 2, false}, error) == 5 &&
            lazyRangeCount({10, 0, -2, true}, error) == 6,
        "lazy ranges count ascending/descending inclusive and exclusive sequences");
    tests.expect(!lazyRangeCount({0, 10, 0, false}, error).has_value() &&
            !lazyRangeCount({0, 10, -1, false}, error).has_value(),
        "zero or directionally invalid steps are diagnosed");
    tests.expect(!rangeMaterializesCollection() &&
            !comprehensionHasImplicitAllocation(BuilderStorage::Stack) &&
            !comprehensionHasImplicitAllocation(BuilderStorage::HeapExplicit),
        "ranges remain values and comprehension allocation belongs to the chosen builder");
    tests.expect(mutationAllowedDuringBorrowedIteration(lending, SourceMutation::Element) &&
            !mutationAllowedDuringBorrowedIteration(lending, SourceMutation::Structural),
        "lending views forbid structural invalidation while permitting compatible element access");

    const std::array<GeneratorLocal, 3> locals = {{{4, 4, true}, {64, 8, false}, {8, 8, true}}};
    const auto frame = computeGeneratorFrame(locals, true);
    tests.expect(frame.has_value() && frame->size == 16 && frame->alignment == 8 && frame->requiresPin,
        "generator frame stores only live-across-yield locals and records pinning");
    const EvaluationContract single{TraversalCategory::SinglePass, true, true};
    const EvaluationContract multi{TraversalCategory::MultiPass, true, true};
    const EvaluationContract reordered{TraversalCategory::SinglePass, true, false};
    tests.expect(mayFuse(single, single) && !mayFuse(single, multi) && !mayFuse(single, reordered),
        "fusion preserves traversal category and observable left-to-right effects");
    return tests.result();
}
