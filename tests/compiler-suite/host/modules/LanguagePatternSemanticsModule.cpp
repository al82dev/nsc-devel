#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/SumTypeSemantics.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::language;

void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LANG-020", "Host.Language", "unit", "PR",
        {"language", "sum-type", "pattern"}, std::move(function)});
}

void registerCompatibleBindingsCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-264", [](TestContext& context) {
        const std::array<PatternBinding, 3> left = {{{"value", "type:Packet", PatternBindingMode::Move},
            {"index", "type:u64", PatternBindingMode::Value},
            {"owner", "type:Session", PatternBindingMode::Reference}}};
        const std::array<PatternBinding, 3> reordered =
            {{{"owner", "type:Session", PatternBindingMode::Reference},
                {"value", "type:Packet", PatternBindingMode::Move},
                {"index", "type:u64", PatternBindingMode::Value}}};
        std::string error = "stale";
        context.expect(orPatternBindingsCompatible(left, reordered, error) && error.empty(),
            "TL-LANG-264 identical name/type/mode maps must accept any alternative order and clear errors");
    });
}

void registerIncompatibleBindingsCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-265", [](TestContext& context) {
        const std::array<PatternBinding, 2> baseline = {{{"value", "type:Packet", PatternBindingMode::Move},
            {"index", "type:u64", PatternBindingMode::Value}}};
        const std::array<PatternBinding, 1> shortAlternative =
            {{{"value", "type:Packet", PatternBindingMode::Move}}};
        std::string error;
        context.expect(!orPatternBindingsCompatible(baseline, shortAlternative, error) &&
                error == "OR-pattern alternatives bind a different number of names",
            "TL-LANG-265 a count mismatch must report the exact count diagnostic");

        auto wrongName = baseline;
        wrongName[1].name = "other";
        auto wrongType = baseline;
        wrongType[0].typeKey = "type:Other";
        auto wrongMode = baseline;
        wrongMode[0].mode = PatternBindingMode::Reference;
        for (const auto& alternative : {wrongName, wrongType, wrongMode})
        {
            context.expect(!orPatternBindingsCompatible(baseline, alternative, error) &&
                    error == "OR-pattern alternatives must bind identical names, types, and modes",
                "TL-LANG-265 name, type and mode mismatches must share the exact structural diagnostic");
        }
    });
}

void registerDuplicateLeftBindingCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-266", [](TestContext& context) {
        const std::array<PatternBinding, 2> duplicateLeft =
            {{{"value", "type:First", PatternBindingMode::Value},
                {"value", "type:Ignored", PatternBindingMode::Move}}};
        const std::array<PatternBinding, 2> matchesFirst =
            {{{"value", "type:First", PatternBindingMode::Value},
                {"value", "type:First", PatternBindingMode::Value}}};
        const std::array<PatternBinding, 2> matchesIgnored =
            {{{"value", "type:Ignored", PatternBindingMode::Move},
                {"value", "type:Ignored", PatternBindingMode::Move}}};
        std::string error;
        context.expect(orPatternBindingsCompatible(duplicateLeft, matchesFirst, error) && error.empty(),
            "TL-LANG-266 emplace must preserve the first duplicate-left binding");
        context.expect(!orPatternBindingsCompatible(duplicateLeft, matchesIgnored, error) &&
                error == "OR-pattern alternatives must bind identical names, types, and modes",
            "TL-LANG-266 the ignored duplicate-left binding must not replace the first entry");
    });
}

void registerClosedMatchCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-267", [](TestContext& context) {
        const std::array<std::string, 3> domain = {"Idle", "Ready", "Failed"};
        const std::array<MatchArmCoverage, 3> exact = {{{{"Idle"}, false, false, {}},
            {{"Ready"}, false, false, {}}, {{"Failed"}, false, false, {}}}};
        const ExhaustivenessResult exactResult = analyzeClosedMatch(domain, exact);
        context.expect(exactResult.exhaustive && exactResult.missingCases.empty() &&
                exactResult.unreachableArms.empty(),
            "TL-LANG-267 exact domain coverage must be exhaustive with no unreachable arms");

        const std::array<MatchArmCoverage, 1> missing = {{{{"Ready"}, false, false, {}}}};
        const ExhaustivenessResult missingResult = analyzeClosedMatch(domain, missing);
        context.expect(!missingResult.exhaustive &&
                missingResult.missingCases == std::vector<std::string>({"Idle", "Failed"}) &&
                missingResult.unreachableArms.empty(),
            "TL-LANG-267 missing cases must retain exact domain order");

        const std::array<MatchArmCoverage, 2> wildcard = {{{{}, true, false, {}},
            {{"Ready"}, false, false, {}}}};
        const ExhaustivenessResult wildcardResult = analyzeClosedMatch(domain, wildcard);
        context.expect(wildcardResult.exhaustive && wildcardResult.missingCases.empty() &&
                wildcardResult.unreachableArms == std::vector<std::size_t>({1}),
            "TL-LANG-267 an unguarded wildcard must cover the domain and make later arms unreachable");

        const std::array<MatchArmCoverage, 2> guarded = {{{{"Idle"}, false, false, {}},
            {{"Ready", "Failed"}, false, true, {}}}};
        const ExhaustivenessResult guardedResult = analyzeClosedMatch(domain, guarded);
        context.expect(!guardedResult.exhaustive &&
                guardedResult.missingCases == std::vector<std::string>({"Ready", "Failed"}) &&
                guardedResult.unreachableArms.empty(),
            "TL-LANG-267 guarded arms must remain useful but contribute no static coverage");

        const std::array<MatchArmCoverage, 4> redundantWildcard = {{{{"Idle"}, false, false, {}},
            {{"Ready"}, false, false, {}}, {{"Failed"}, false, false, {}}, {{}, true, false, {}}}};
        context.expect(analyzeClosedMatch(domain, redundantWildcard).unreachableArms ==
                std::vector<std::size_t>({3}),
            "TL-LANG-267 a wildcard after complete domain coverage must be unreachable");

        MatchArmCoverage exactUnion;
        exactUnion.listLengths = {{1, false}, {2, false}};
        const std::array<MatchArmCoverage, 1> finiteListUnion = {exactUnion};
        const ExhaustivenessResult finiteListResult = analyzeListMatch(finiteListUnion);
        context.expect(!finiteListResult.exhaustive && finiteListResult.missingCases.empty() &&
                finiteListResult.unreachableArms.empty(),
            "TL-LANG-267 exact OR-list lengths must remain a useful finite union");

        MatchArmCoverage exhaustiveUnion;
        exhaustiveUnion.listLengths = {{0, false}, {1, true}};
        const std::array<MatchArmCoverage, 1> exactAndRest = {exhaustiveUnion};
        const ExhaustivenessResult exactAndRestResult = analyzeListMatch(exactAndRest);
        context.expect(exactAndRestResult.exhaustive &&
                exactAndRestResult.missingCases.empty() &&
                exactAndRestResult.unreachableArms.empty(),
            "TL-LANG-267 exact zero OR rest-from-one must cover every list length");

        MatchArmCoverage coveredUnion;
        coveredUnion.listLengths = {{1, false}, {2, true}};
        MatchArmCoverage redundantUnion;
        redundantUnion.listLengths = {{2, false}, {3, true}};
        const std::array<MatchArmCoverage, 2> redundantLists = {
            coveredUnion, redundantUnion};
        context.expect(analyzeListMatch(redundantLists).unreachableArms ==
                std::vector<std::size_t>({1}),
            "TL-LANG-267 an OR-list arm is unreachable only when every length domain is covered");
    });
}

void registerLetElseCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-268", [](TestContext& context) {
        context.expect(letElseGuaranteesInitialization(true) &&
                !letElseGuaranteesInitialization(false),
            "TL-LANG-268 initialization guarantee must equal else divergence for both inputs");
    });
}

void registerCUnionReadCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-269", [](TestContext& context) {
        context.expect(!canReadCUnionMember(false, false) && canReadCUnionMember(false, true) &&
                canReadCUnionMember(true, false) && canReadCUnionMember(true, true),
            "TL-LANG-269 C union reads must accept exactly proof OR unsafe context");
    });
}
}

void registerLanguagePatternSemanticsModule(TestRegistry& registry)
{
    registerCompatibleBindingsCase(registry);
    registerIncompatibleBindingsCase(registry);
    registerDuplicateLeftBindingCase(registry);
    registerClosedMatchCase(registry);
    registerLetElseCase(registry);
    registerCUnionReadCase(registry);
}
}
