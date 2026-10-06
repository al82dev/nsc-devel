#pragma once

#include <cstddef>

namespace nativesharp::frontend
{
struct ProjectEscapePropagationStatistics final
{
    std::size_t cells = 0;
    std::size_t edges = 0;
    std::size_t argumentOriginsClassified = 0;
    std::size_t cellsEvaluated = 0;
    std::size_t edgeEvaluations = 0;
    std::size_t summaryUpdates = 0;
    std::size_t referenceOriginEvaluations = 0;
    std::size_t referenceContinuationFrames = 0;
    std::size_t maximumReferenceContinuationDepth = 0;
    std::size_t legacyGlobalCallRescans = 0;
    std::size_t recursiveVariableArrayFallbacks = 0;
    std::size_t recursiveReferenceOriginFallbacks = 0;
};
}
