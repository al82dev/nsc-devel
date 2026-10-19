#pragma once

namespace nativesharp::language
{
enum class FusionRejection
{
    None,
    EagerBoundary,
    EffectReordering,
    TraversalUpgrade,
    RepeatedEvaluation,
    ShortCircuitLost,
    BorrowedItemBuffered
};
}
