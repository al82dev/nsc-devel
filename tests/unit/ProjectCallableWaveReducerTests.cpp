#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/ProjectCallableAnalysisResult.h"
#include "nativesharp/Frontend/ProjectCallableWaveReducer.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "../support/TestContext.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
nativesharp::frontend::ProjectCallableAnalysisResult makeResult(
    std::size_t callable,
    std::uint32_t offset,
    std::string message,
    std::size_t discovery)
{
    using namespace nativesharp;
    diagnostics::DiagnosticEngine local;
    local.report(
        diagnostics::DiagnosticId::InvalidCall,
        diagnostics::DiagnosticSeverity::Error,
        SourceLocation(FileId(1), offset),
        std::move(message));
    frontend::ProjectCallableAnalysisResult result(
        callable,
        0,
        std::make_unique<frontend::ProjectMethodBodyModel>(),
        std::move(local));
    result.addDiscoveredCallable(discovery);
    result.normalizeDiscoveredCallables();
    return result;
}

std::vector<std::string> messages(
    const nativesharp::diagnostics::DiagnosticEngine& diagnostics)
{
    std::vector<std::string> result;
    for (const auto& diagnostic : diagnostics.diagnostics())
        result.push_back(diagnostic.message());
    return result;
}
}

int main()
{
    using namespace nativesharp;
    using namespace nativesharp::frontend;
    using nativesharp::tests::TestContext;

    TestContext tests;
    ProjectMethodBodyModel forwardModel;
    diagnostics::DiagnosticEngine forwardDiagnostics;
    std::vector<ProjectCallableAnalysisResult> forward;
    forward.push_back(makeResult(4, 40, "late source", 9));
    forward.push_back(makeResult(2, 10, "early source", 7));
    const std::vector<std::size_t> forwardDiscoveries =
        ProjectCallableWaveReducer::reduce(
            forwardModel, std::move(forward), forwardDiagnostics);

    ProjectMethodBodyModel reverseModel;
    diagnostics::DiagnosticEngine reverseDiagnostics;
    std::vector<ProjectCallableAnalysisResult> reverse;
    reverse.push_back(makeResult(2, 10, "early source", 7));
    reverse.push_back(makeResult(4, 40, "late source", 9));
    const std::vector<std::size_t> reverseDiscoveries =
        ProjectCallableWaveReducer::reduce(
            reverseModel, std::move(reverse), reverseDiagnostics);

    tests.expect(messages(forwardDiagnostics) == messages(reverseDiagnostics),
        "reversing completion order preserves exact diagnostic order");
    tests.expect(messages(forwardDiagnostics) ==
            std::vector<std::string>{"early source", "late source"},
        "wave diagnostics publish in canonical source order");
    tests.expect(forwardDiscoveries == reverseDiscoveries &&
            forwardDiscoveries == std::vector<std::size_t>{7, 9},
        "wave discoveries are stable and canonical");
    tests.expect(forwardModel.scopes().empty() && reverseModel.scopes().empty(),
        "empty callable shards merge without shared mutable state");
    tests.expect(forwardModel.callableAnalysisStatistics().wavesReduced == 1 &&
            forwardModel.callableAnalysisStatistics().resultsMerged == 2 &&
            forwardModel.callableAnalysisStatistics().discoveriesPublished == 2,
        "wave reducer publishes structural callable telemetry");
    return tests.result();
}
