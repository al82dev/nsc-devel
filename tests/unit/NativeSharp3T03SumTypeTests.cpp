#include "nativesharp/Language/SumTypeSemantics.h"

#include "../support/TestContext.h"

#include <array>
#include <string>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;

    const std::array<VariantCase, 2> resultCases = {{{"Ok", 4, 4}, {"Error", 16, 8}}};
    const auto layout = computeWindowsX64VariantLayout(resultCases);
    tests.expect(layout.has_value() && layout->tagSize == 1 && layout->payloadOffset == 8 &&
            layout->size == 24 && layout->alignment == 8,
        "variant payload is in-place with the smallest tag and Windows x64 alignment");
    tests.expect(!computeWindowsX64VariantLayout({}).has_value(),
        "an empty variant is rejected instead of acquiring an impossible runtime state");

    std::string error;
    const std::array<PatternBinding, 1> byMove = {{{"value", "type:Packet", PatternBindingMode::Move}}};
    const std::array<PatternBinding, 1> byRef = {{{"value", "type:Packet", PatternBindingMode::Reference}}};
    tests.expect(orPatternBindingsCompatible(byMove, byMove, error) &&
            !orPatternBindingsCompatible(byMove, byRef, error),
        "OR-pattern alternatives preserve identical ownership bindings");

    const std::array<std::string, 3> domain = {"Idle", "Ready", "Failed"};
    const std::array<MatchArmCoverage, 3> complete = {{{{"Idle"}, false, false, {}},
        {{"Ready"}, false, false, {}}, {{"Failed"}, false, false, {}}}};
    const auto completeResult = analyzeClosedMatch(domain, complete);
    tests.expect(completeResult.exhaustive && completeResult.missingCases.empty(),
        "closed match reports exhaustive case coverage");
    const std::array<MatchArmCoverage, 2> guarded = {{{{"Idle"}, false, false, {}},
        {{"Ready", "Failed"}, false, true, {}}}};
    const auto guardedResult = analyzeClosedMatch(domain, guarded);
    tests.expect(!guardedResult.exhaustive && guardedResult.missingCases.size() == 2,
        "a dynamic guard never contributes to static exhaustiveness");
    const std::array<MatchArmCoverage, 2> unreachable = {{{{}, true, false, {}},
        {{"Ready"}, false, false, {}}}};
    tests.expect(analyzeClosedMatch(domain, unreachable).unreachableArms ==
            std::vector<std::size_t>({1}),
        "an arm after an unguarded wildcard is unreachable");
    const std::array<MatchArmCoverage, 4> redundantWildcard = {{{{"Idle"}, false, false, {}},
        {{"Ready"}, false, false, {}}, {{"Failed"}, false, false, {}}, {{}, true, false, {}}}};
    tests.expect(analyzeClosedMatch(domain, redundantWildcard).unreachableArms ==
            std::vector<std::size_t>({3}),
        "a wildcard after complete closed-domain coverage is unreachable");

    tests.expect(letElseGuaranteesInitialization(true) && !letElseGuaranteesInitialization(false),
        "let-else requires a diverging failure branch");
    tests.expect(canReadCUnionMember(true, false) && canReadCUnionMember(false, true) &&
            !canReadCUnionMember(false, false),
        "C union reads require an active-member proof or an unsafe context");
    tests.expect(variantMetadataIdentity("Demo.Result", resultCases) ==
            "Demo.Result|variant|2:Ok;5:Error;",
        "variant case catalog has a deterministic cross-module metadata identity");

    return tests.result();
}
