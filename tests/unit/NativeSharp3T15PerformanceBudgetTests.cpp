#include "nativesharp/Language/IntegrationQuality.h"

#include "../support/TestContext.h"

#include <set>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;

    const auto profiles = nativeSharp3PerformanceBudgets();
    std::set<PerformanceCorpus> corpora;
    for (const auto& profile : profiles) corpora.insert(profile.corpus);
    tests.expect(profiles.size() == 6 && corpora.size() == 6,
        "budgets cover giant, generic, macro, fanout, async, and diagnostic corpora exactly once");
    tests.expect(profiles[0].referenceHardware.find("Windows-x64") != std::string::npos &&
            profiles[0].minimumSamples == 7 && profiles[0].tolerancePercent == 5,
        "reference hardware, sample floor, and tolerance are explicit");

    PerformanceBudget measured = profiles[0].maximum;
    measured.frontendMilliseconds += measured.frontendMilliseconds / 25;
    measured.backendMilliseconds = 999999;
    measured.toolchainMilliseconds = 999999;
    const auto tolerated = assessLanguagePerformance(measured, profiles[0], 7);
    tests.expect(tolerated.passed && tolerated.backendAndToolchainExcluded,
        "a measured value inside tolerance passes and backend/toolchain time is separated");

    measured.frontendMilliseconds = profiles[0].maximum.frontendMilliseconds * 2;
    const auto slow = assessLanguagePerformance(measured, profiles[0], 7);
    tests.expect(!slow.passed && slow.violations.size() == 1 &&
            slow.violations[0].find("frontend time") != std::string::npos,
        "frontend regressions fail independently with a causal metric");

    measured = profiles[1].maximum;
    measured.monomorphizations *= 2;
    const auto genericExplosion = assessLanguagePerformance(measured, profiles[1], 7);
    tests.expect(!genericExplosion.passed &&
            genericExplosion.violations[0].find("monomorphization") != std::string::npos,
        "generic explosions are not hidden inside total compile time");

    measured = profiles[4].maximum;
    measured.mirInstructions *= 2;
    measured.codeBytes *= 2;
    const auto asyncExplosion = assessLanguagePerformance(measured, profiles[4], 7);
    tests.expect(!asyncExplosion.passed && asyncExplosion.violations.size() == 2,
        "async frame MIR and code-size regressions are reported separately");

    const auto undersampled = assessLanguagePerformance(profiles[0].maximum, profiles[0], 2);
    tests.expect(!undersampled.passed &&
            undersampled.violations[0].find("sample count") != std::string::npos,
        "a favorable undersampled run cannot certify a budget");

    auto unbounded = profiles[0];
    unbounded.tolerancePercent = 50;
    tests.expect(!assessLanguagePerformance(unbounded.maximum, unbounded, 7).passed,
        "tolerance is itself bounded and cannot waive arbitrary regressions");
    return tests.result();
}
