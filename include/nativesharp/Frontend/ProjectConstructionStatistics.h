#pragma once

#include <cstddef>

namespace nativesharp::frontend
{
// Deterministic structural counters for the MethodBody -> Construction hand-off. They are part of
// directed assurance only: no timing value enters dumps, metadata, hashes, or ordinary output.
struct ProjectConstructionStatistics final
{
    std::size_t typedSitesConsumed = 0;
    std::size_t typedArgumentsConsumed = 0;
    std::size_t unsupportedContextNodesVisited = 0;
    std::size_t bodyAstNodesRescanned = 0;
};
}
