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

bool validatePrioritizedInteractionMatrix(
    std::span<const InteractionCase> cases,
    std::string& error)
{
    if (!validateInteractionMatrix(cases, error)) return false;
    bool hasTriple = false;
    for (const InteractionCase& item : cases)
    {
        if (item.risk == InteractionRisk::Medium || !item.minimizesOnFailure ||
            (!item.changesMir && !item.changesOwnership))
        {
            error = "prioritized interaction cases require high risk, compiler impact, and minimization";
            return false;
        }
        hasTriple = hasTriple || item.areas.size() >= 3;
    }
    if (!hasTriple)
    {
        error = "prioritized interaction matrix requires at least one high-risk feature triple";
        return false;
    }
    return true;
}

std::vector<InteractionCase> buildNativeSharp3InteractionMatrix(std::uint64_t seedBase)
{
    if (seedBase == 0) seedBase = 0x4e533300ULL;
    return {
        {"variant-move-drop", {FeatureArea::Variants, FeatureArea::Ownership}, seedBase + 1,
            InteractionRisk::Critical, true, true, true},
        {"generic-trait-associated", {FeatureArea::Generics, FeatureArea::Traits}, seedBase + 2,
            InteractionRisk::High, true, false, true},
        {"borrow-async-error", {FeatureArea::Ownership, FeatureArea::Coroutines, FeatureArea::Errors},
            seedBase + 3, InteractionRisk::Critical, true, true, true},
        {"module-ffi-contract", {FeatureArea::Modules, FeatureArea::Ffi}, seedBase + 4,
            InteractionRisk::Critical, true, true, true},
        {"derive-module-fingerprint", {FeatureArea::Metaprogramming, FeatureArea::Modules}, seedBase + 5,
            InteractionRisk::High, true, false, true},
        {"closure-generic-borrow", {FeatureArea::Closures, FeatureArea::Generics, FeatureArea::Ownership},
            seedBase + 6, InteractionRisk::Critical, true, true, true},
        {"iterator-async-numeric", {FeatureArea::Iteration, FeatureArea::Coroutines, FeatureArea::Numerics},
            seedBase + 7, InteractionRisk::High, true, true, true}};
}

std::optional<InteractionCase> minimizeInteractionCase(
    const InteractionCase& failing,
    std::span<const FeatureArea> essentialAreas,
    std::string& error)
{
    error.clear();
    if (!failing.minimizesOnFailure || essentialAreas.size() < 2)
    {
        error = "interaction failure requires at least two essential areas and minimization support";
        return std::nullopt;
    }
    for (const FeatureArea essential : essentialAreas)
    {
        if (std::ranges::find(failing.areas, essential) == failing.areas.end())
        {
            error = "minimized interaction contains an area absent from the failing case";
            return std::nullopt;
        }
    }
    InteractionCase result = failing;
    result.identity += "-min";
    result.areas.assign(essentialAreas.begin(), essentialAreas.end());
    return result;
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

std::vector<WholeProgramWitness> buildNativeSharp3WholeProgramCorpus()
{
    using Kind = WholeProgramWitness::Kind;
    return {
        {"parser", true, true, false, Kind::Parser, true, true, true, true, true, 4, 42},
        {"arena", true, true, false, Kind::ArenaAllocator, true, true, true, true, true, 3, 42},
        {"job-graph", true, true, false, Kind::JobGraph, true, true, true, true, true, 5, 42},
        {"serializer", true, true, false, Kind::BinarySerializer, true, true, true, true, true, 4, 42},
        {"dll-host-plugin", true, true, false, Kind::DllHostPlugin, true, true, true, true, false, 6, 42},
        {"async-server", true, true, false, Kind::AsyncServer, true, true, true, true, false, 5, 42}};
}

bool validateNativeSharp3WholeProgramCorpus(
    std::span<const WholeProgramWitness> programs,
    std::string& error)
{
    if (!validateWholeProgramCorpus(programs, error)) return false;
    std::set<WholeProgramWitness::Kind> kinds;
    for (const WholeProgramWitness& program : programs)
    {
        if (!program.debugAndRelease || !program.cleanAndIncremental ||
            !program.ltoAndNoLto || !program.sanitizerReady ||
            program.minimumSourceUnits == 0 || program.expectedExitCode != 42)
        {
            error = "whole-program witness lacks a required build profile or observable result";
            return false;
        }
        if (!kinds.insert(program.kind).second)
        {
            error = "whole-program corpus contains a duplicate witness kind";
            return false;
        }
    }
    if (kinds.size() != 6)
    {
        error = "whole-program corpus must contain parser, arena, job graph, serializer, DLL, and async witnesses";
        return false;
    }
    const std::size_t noHeapPrograms = static_cast<std::size_t>(std::ranges::count_if(
        programs, [](const WholeProgramWitness& program) { return program.partialNoHeap; }));
    if (noHeapPrograms < 4)
    {
        error = "whole-program corpus requires at least four partial noheap profiles";
        return false;
    }
    return true;
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

std::vector<PerformanceBudgetProfile> nativeSharp3PerformanceBudgets()
{
    constexpr std::string_view hardware =
        "Windows-x64|12-logical-cores|32GiB|NVMe|clang-cl-23|NSLink-3";
    return {
        {"giant-file", PerformanceCorpus::GiantFile,
            {2500, 768ULL * 1024 * 1024, 600000, 2500, 32ULL * 1024 * 1024}, 5,
            std::string(hardware), 7},
        {"generic-heavy", PerformanceCorpus::GenericHeavy,
            {3000, 1024ULL * 1024 * 1024, 750000, 12000, 48ULL * 1024 * 1024}, 5,
            std::string(hardware), 7},
        {"macro-heavy", PerformanceCorpus::MacroHeavy,
            {2800, 768ULL * 1024 * 1024, 650000, 3500, 36ULL * 1024 * 1024}, 5,
            std::string(hardware), 7},
        {"module-fanout", PerformanceCorpus::ModuleFanout,
            {3500, 1024ULL * 1024 * 1024, 800000, 6000, 56ULL * 1024 * 1024}, 5,
            std::string(hardware), 7},
        {"async-frames", PerformanceCorpus::AsyncFrames,
            {2600, 768ULL * 1024 * 1024, 700000, 4500, 40ULL * 1024 * 1024}, 5,
            std::string(hardware), 7},
        {"pathological-diagnostics", PerformanceCorpus::PathologicalDiagnostics,
            {1800, 512ULL * 1024 * 1024, 250000, 1000, 8ULL * 1024 * 1024}, 5,
            std::string(hardware), 7}};
}

PerformanceAssessment assessLanguagePerformance(
    const PerformanceBudget& measured,
    const PerformanceBudgetProfile& profile,
    std::size_t sampleCount) noexcept
{
    PerformanceAssessment result;
    if (profile.identity.empty() || profile.referenceHardware.empty())
        result.violations.push_back("performance profile requires identity and reference hardware");
    if (profile.tolerancePercent > 25)
        result.violations.push_back("performance tolerance exceeds the permitted 25 percent ceiling");
    if (sampleCount < profile.minimumSamples)
        result.violations.push_back("performance sample count is below the profile minimum");

    const auto limit = [&](std::size_t maximum)
    {
        return maximum + (maximum * profile.tolerancePercent) / 100U;
    };
    if (measured.frontendMilliseconds > limit(profile.maximum.frontendMilliseconds))
        result.violations.push_back("frontend time exceeded its language budget");
    if (measured.peakMemoryBytes > limit(profile.maximum.peakMemoryBytes))
        result.violations.push_back("frontend peak memory exceeded its language budget");
    if (measured.mirInstructions > limit(profile.maximum.mirInstructions))
        result.violations.push_back("MIR size exceeded its language budget");
    if (measured.monomorphizations > limit(profile.maximum.monomorphizations))
        result.violations.push_back("monomorphization count exceeded its language budget");
    if (measured.codeBytes > limit(profile.maximum.codeBytes))
        result.violations.push_back("generated code size exceeded its language budget");
    result.passed = result.violations.empty();
    return result;
}

bool diagnosticPassesQualityGate(const DiagnosticQuality& diagnostic, std::string& error)
{
    error.clear();
    if (!diagnostic.code.starts_with("NS3") || diagnostic.english.empty() ||
        diagnostic.spanish.empty() || !diagnostic.precisePrimarySpan ||
        !diagnostic.fixItPreservesSemantics || diagnostic.cascadeCount > 2)
    { error = "diagnostic quality gate requires stable code, localization, precise span, safe fix-it, and bounded cascades"; return false; }
    if (!diagnostic.primarySpan.file.empty() &&
        (diagnostic.primarySpan.line == 0 || diagnostic.primarySpan.column == 0 ||
         diagnostic.primarySpan.endLine < diagnostic.primarySpan.line ||
         (diagnostic.primarySpan.endLine == diagnostic.primarySpan.line &&
          diagnostic.primarySpan.endColumn < diagnostic.primarySpan.column)))
    {
        error = "diagnostic primary span is invalid";
        return false;
    }
    if (diagnostic.fixIt.has_value() &&
        (diagnostic.fixIt->replacement.empty() || !diagnostic.fixIt->preservesSemantics ||
         !diagnostic.fixIt->compiles))
    {
        error = "diagnostic fix-it must be nonempty, semantics-preserving, and compilable";
        return false;
    }
    if (diagnostic.macroBacktrace.size() > 4)
    {
        error = "diagnostic macro backtrace is not compact";
        return false;
    }
    return true;
}

bool validateDiagnosticQualityCatalog(
    std::span<const DiagnosticQuality> diagnostics,
    std::size_t terminalWidth,
    std::string& error)
{
    error.clear();
    if (terminalWidth < 40)
    {
        error = "diagnostic terminal width must be at least 40 columns";
        return false;
    }
    std::unordered_set<std::string> codes;
    std::set<DiagnosticArea> areas;
    for (const DiagnosticQuality& diagnostic : diagnostics)
    {
        if (!diagnosticPassesQualityGate(diagnostic, error)) return false;
        if (diagnostic.primarySpan.file.empty())
        {
            error = "diagnostic catalog entries require a physical primary span";
            return false;
        }
        if (!codes.insert(diagnostic.code).second)
        {
            error = "diagnostic catalog contains a duplicate stable code";
            return false;
        }
        areas.insert(diagnostic.area);
        if (renderDiagnosticForTerminal(diagnostic, false, terminalWidth).size() > terminalWidth ||
            renderDiagnosticForTerminal(diagnostic, true, terminalWidth).size() > terminalWidth)
        {
            error = "diagnostic renderer exceeded terminal width";
            return false;
        }
    }
    if (areas.size() != 5)
    {
        error = "diagnostic catalog must cover borrow, trait, match, async, and module errors";
        return false;
    }
    return true;
}

std::string renderDiagnosticForTerminal(
    const DiagnosticQuality& diagnostic,
    bool spanish,
    std::size_t terminalWidth)
{
    if (terminalWidth == 0) return {};
    std::string result = diagnostic.code + ": " + (spanish ? diagnostic.spanish : diagnostic.english);
    if (result.size() <= terminalWidth) return result;
    if (terminalWidth <= 3) return result.substr(0, terminalWidth);
    result.resize(terminalWidth - 3);
    result += "...";
    return result;
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
