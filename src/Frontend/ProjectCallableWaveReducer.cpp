#include "nativesharp/Frontend/ProjectCallableWaveReducer.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Frontend/ProjectCallableAnalysisResult.h"
#include "nativesharp/Frontend/ProjectCallableResultMerger.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include <algorithm>
#include <tuple>
#include <utility>

namespace nativesharp::frontend
{
std::vector<std::size_t> ProjectCallableWaveReducer::reduce(
    ProjectMethodBodyModel& destination,
    std::vector<ProjectCallableAnalysisResult>&& results,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::ranges::sort(results, [](const auto& left, const auto& right) {
        return std::tuple(left.waveOrdinal(), left.callableOrdinal()) <
            std::tuple(right.waveOrdinal(), right.callableOrdinal());
    });
    ++destination.callableAnalysisStatistics_.wavesReduced;

    std::vector<std::size_t> discoveries;
    std::vector<diagnostics::DiagnosticEngine> localDiagnostics;
    localDiagnostics.reserve(results.size());
    for (ProjectCallableAnalysisResult& result : results)
    {
        discoveries.insert(
            discoveries.end(),
            result.discoveredCallables().begin(),
            result.discoveredCallables().end());
        localDiagnostics.push_back(std::move(result.diagnostics()));
        ProjectCallableResultMerger::merge(destination, std::move(result));
        // The merged state now lives in the destination model: free the shard before the next
        // merge instead of keeping every shard of the wave alive until the reduction ends.
        result.releaseShard();
    }
    diagnostics.mergeDeterministically(localDiagnostics);

    std::ranges::sort(discoveries);
    discoveries.erase(
        std::unique(discoveries.begin(), discoveries.end()), discoveries.end());
    destination.callableAnalysisStatistics_.discoveriesPublished += discoveries.size();
    return discoveries;
}
}
