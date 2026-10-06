#pragma once

#include <cstddef>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
class ProjectCallableAnalysisResult;
class ProjectMethodBodyModel;

class ProjectCallableWaveReducer final
{
public:
    [[nodiscard]] static std::vector<std::size_t> reduce(
        ProjectMethodBodyModel& destination,
        std::vector<ProjectCallableAnalysisResult>&& results,
        diagnostics::DiagnosticEngine& diagnostics);
};
}
