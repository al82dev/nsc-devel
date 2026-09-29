#include "nativesharp/Language/IntegrationQuality.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <unordered_set>

namespace nativesharp::language
{
bool validateInteractionMatrix(std::span<const InteractionCase> cases, std::string& error)
{
    error.clear();
    std::unordered_set<std::string> identities;
    std::set<std::pair<FeatureArea, FeatureArea>> pairs;
    for (const auto& item : cases)
    {
        if (item.identity.empty() || item.areas.size() < 2 || item.deterministicSeed == 0)
        { error = "interaction cases require identity, at least two areas, and a fixed nonzero seed"; return false; }
        if (!identities.insert(item.identity).second)
        { error = "duplicate feature interaction identity"; return false; }
        for (std::size_t left = 0; left < item.areas.size(); ++left)
            for (std::size_t right = left + 1; right < item.areas.size(); ++right)
            {
                FeatureArea a = item.areas[left];
                FeatureArea b = item.areas[right];
                if (a > b) std::swap(a, b);
                pairs.emplace(a, b);
            }
    }
    const std::pair<FeatureArea, FeatureArea> required[] = {
        {FeatureArea::Variants, FeatureArea::Ownership},
        {FeatureArea::Generics, FeatureArea::Traits},
        {FeatureArea::Ownership, FeatureArea::Coroutines},
        {FeatureArea::Errors, FeatureArea::Coroutines},
        {FeatureArea::Modules, FeatureArea::Ffi},
        {FeatureArea::Metaprogramming, FeatureArea::Modules}};
    for (const auto& pair : required)
        if (!pairs.contains(pair)) { error = "high-risk feature pair is missing from integration matrix"; return false; }
    return true;
}

bool validateWholeProgramCorpus(
    std::span<const WholeProgramWitness> programs,
    std::string& error)
{
    error.clear();
    std::unordered_set<std::string> identities;
    for (const auto& program : programs)
    {
        if (program.identity.empty() || !program.deterministic || !program.supportsIncremental ||
            program.requiresSdk || !identities.insert(program.identity).second)
        { error = "whole-program witnesses must be unique, deterministic, incremental, and SDK-independent"; return false; }
    }
    return !programs.empty();
}

bool withinPerformanceBudget(
    const PerformanceBudget& measured,
    const PerformanceBudget& maximum) noexcept
{
    return measured.frontendMilliseconds <= maximum.frontendMilliseconds &&
        measured.peakMemoryBytes <= maximum.peakMemoryBytes &&
        measured.mirInstructions <= maximum.mirInstructions &&
        measured.monomorphizations <= maximum.monomorphizations &&
        measured.codeBytes <= maximum.codeBytes;
}

bool diagnosticPassesQualityGate(const DiagnosticQuality& diagnostic, std::string& error)
{
    error.clear();
    if (!diagnostic.code.starts_with("NS3") || diagnostic.english.empty() ||
        diagnostic.spanish.empty() || !diagnostic.precisePrimarySpan ||
        !diagnostic.fixItPreservesSemantics || diagnostic.cascadeCount > 2)
    { error = "diagnostic quality gate requires stable code, localization, precise span, safe fix-it, and bounded cascades"; return false; }
    return true;
}

std::string semanticDebugName(
    std::string_view owner,
    std::string_view role,
    std::size_t stableOrdinal)
{
    std::string result(owner);
    result += "$";
    for (char value : role)
        result.push_back(std::isalnum(static_cast<unsigned char>(value)) != 0 ? value : '_');
    result += "$" + std::to_string(stableOrdinal);
    return result;
}

bool hiddenCostAuditPasses(std::span<const CostEvent> costs, std::string& error)
{
    error.clear();
    for (const CostEvent& cost : costs)
        if (!cost.explicitlyRequested)
        { error = "hidden generated cost in " + cost.sourceConstruct; return false; }
    return true;
}

std::string_view semanticToolingSchemaVersion() noexcept { return "nsc-semantic-tooling-v1"; }
}
