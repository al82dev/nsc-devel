#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/ProjectCallableAnalysisResult.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "../support/TestContext.h"

#include <memory>
#include <utility>

int main()
{
    using namespace nativesharp;
    using namespace nativesharp::diagnostics;
    using namespace nativesharp::frontend;
    using nativesharp::tests::TestContext;

    TestContext tests;
    DiagnosticEngine diagnostics;
    diagnostics.report(
        DiagnosticId::InvalidCall,
        DiagnosticSeverity::Error,
        SourceLocation(),
        "callable-local diagnostic");
    ProjectCallableAnalysisResult result(
        17, 3, std::make_unique<ProjectMethodBodyModel>(), std::move(diagnostics));
    result.addDiscoveredCallable(9);
    result.addDiscoveredCallable(4);
    result.addDiscoveredCallable(9);
    result.normalizeDiscoveredCallables();

    tests.expect(result.callableOrdinal() == 17, "result preserves stable callable ordinal");
    tests.expect(result.waveOrdinal() == 3, "result preserves stable wave ordinal");
    tests.expect(result.diagnostics().errorCount() == 1,
        "result owns callable-local diagnostics");
    tests.expect(result.discoveredCallables().size() == 2,
        "result removes duplicate discoveries");
    tests.expect(result.discoveredCallables()[0] == 4 &&
            result.discoveredCallables()[1] == 9,
        "result publishes discoveries in canonical order");

    ProjectCallableAnalysisResult moved(std::move(result));
    tests.expect(moved.callableOrdinal() == 17 && moved.shard().scopes().empty(),
        "result transfers exclusive shard ownership without copying");
    return tests.result();
}
