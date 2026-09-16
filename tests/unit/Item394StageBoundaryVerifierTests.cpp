#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/Frontend/StageBoundaryVerifier.h"
#include "nativesharp/HIR/HirNode.h"

#include "../support/TestContext.h"

#include <array>
#include <string>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::frontend;

    FrontendCompilation compilation;
    tests.expect(compilation.compileBuffer(
                     "item394.ns", "int answer() { return 42; }", false),
        "valid compilation crosses every verified stage boundary");

    constexpr std::array expected = {StageBoundary::HirToMir, StageBoundary::MirToCfg,
        StageBoundary::CfgToDominance, StageBoundary::DominanceToSsa,
        StageBoundary::SsaToOptimization, StageBoundary::OptimizationToBackend};
    tests.expect(compilation.verifiedStageBoundaries().size() == expected.size(),
        "the pipeline records all six mandatory local boundaries");
    for (std::size_t index = 0;
         index < expected.size() && index < compilation.verifiedStageBoundaries().size(); ++index)
    {
        tests.expect(compilation.verifiedStageBoundaries()[index] == expected[index],
            "stage boundaries execute in producer/consumer order");
    }

    StageBoundaryVerifier verifier;
    tests.expect(!verifier.verify(
                     StageBoundary::HirToMir,
                     static_cast<const nativesharp::hir::HirNode*>(nullptr)),
        "local HIR boundary rejects a missing producer value");
    tests.expect(verifier.boundary() == StageBoundary::HirToMir &&
            verifier.error().find("stage boundary 'hir->mir'") != std::string::npos,
        "boundary failure names the exact producer/consumer edge");
    tests.expect(stageBoundaryName(StageBoundary::OptimizationToBackend) ==
            "optimization->backend",
        "boundary names are stable for diagnostics and telemetry");

    return tests.result();
}
