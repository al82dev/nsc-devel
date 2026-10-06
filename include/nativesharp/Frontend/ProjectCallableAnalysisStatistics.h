#pragma once

#include <cstddef>

namespace nativesharp::frontend
{
struct ProjectCallableAnalysisStatistics final
{
    std::size_t callablesQueued = 0;
    std::size_t callablesAnalyzed = 0;
    std::size_t callablesSkipped = 0;
    std::size_t callablesReused = 0;
    std::size_t shardsCreated = 0;
    std::size_t resultsMerged = 0;
    std::size_t wavesReduced = 0;
    std::size_t discoveriesPublished = 0;
    std::size_t requestedWorkers = 1;
    std::size_t effectiveWorkers = 1;
    std::size_t maximumWaveWidth = 0;
    std::size_t synchronousWaves = 0;
    std::size_t parallelWaves = 0;
};
}
