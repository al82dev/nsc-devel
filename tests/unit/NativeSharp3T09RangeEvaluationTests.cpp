#include "nativesharp/Language/IterationSemantics.h"

#include "../support/TestContext.h"

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;

    const EvaluationContract single{TraversalCategory::SinglePass, true, true};
    const EvaluationContract multi{TraversalCategory::MultiPass, true, true};
    const EvaluationContract eager{TraversalCategory::SinglePass, false, true};
    const EvaluationContract reordered{TraversalCategory::SinglePass, true, false};

    const FusionDecision ordinary = analyzeFusion(FusionRequest{single, single});
    tests.expect(ordinary.allowed && ordinary.rejection == FusionRejection::None &&
            ordinary.resultCategory == TraversalCategory::SinglePass,
        "lazy single-pass adapters fuse without changing traversal category");
    tests.expect(analyzeFusion(FusionRequest{eager, single}).rejection ==
            FusionRejection::EagerBoundary,
        "an eager source or adapter forms an observable materialization boundary");
    tests.expect(analyzeFusion(FusionRequest{single, reordered}).rejection ==
            FusionRejection::EffectReordering,
        "fusion rejects an adapter that does not preserve left-to-right effects");
    tests.expect(analyzeFusion(FusionRequest{single, multi}).rejection ==
            FusionRejection::TraversalUpgrade,
        "a single-pass source cannot be upgraded to multi-pass by fusion");

    FusionRequest repeated{single, single};
    repeated.adapter.repeatsElements = true;
    repeated.fusedEvaluatesEachSourceAtMostOnce = false;
    tests.expect(analyzeFusion(repeated).rejection == FusionRejection::RepeatedEvaluation,
        "effectful single-pass elements cannot be evaluated more than once");

    FusionRequest shortCircuit{single, single};
    shortCircuit.adapter.mayShortCircuit = true;
    shortCircuit.fusedPreservesShortCircuit = false;
    tests.expect(analyzeFusion(shortCircuit).rejection == FusionRejection::ShortCircuitLost,
        "take/first/any-style termination must remain short-circuiting after fusion");

    FusionRequest borrowed{single, single};
    borrowed.source.yieldsBorrowedItems = true;
    borrowed.adapter.buffersElements = true;
    tests.expect(analyzeFusion(borrowed).rejection == FusionRejection::BorrowedItemBuffered,
        "a lending iterator cannot fuse through an adapter that buffers borrowed items");

    const FusionDecision reusable = analyzeFusion(FusionRequest{multi, multi});
    tests.expect(reusable.allowed && reusable.resultCategory == TraversalCategory::MultiPass &&
            mayFuse(multi, multi) && !mayFuse(single, multi),
        "multi-pass remains multi-pass and the compatibility helper uses the causal analysis");

    return tests.result();
}
