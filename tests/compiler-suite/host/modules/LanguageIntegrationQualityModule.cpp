#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/IntegrationQuality.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
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
    registry.add(TestCase{std::move(id), "CL-LANG-010", "Host.Language", "unit", "PR",
        {"language", "integration-quality"}, std::move(function)});
}

std::vector<InteractionCase> minimalInteractionMatrix()
{
    return {
        {"variant-ownership", {FeatureArea::Variants, FeatureArea::Ownership}, 1},
        {"generic-trait", {FeatureArea::Generics, FeatureArea::Traits}, 2},
        {"ownership-coroutine", {FeatureArea::Ownership, FeatureArea::Coroutines}, 3},
        {"error-coroutine", {FeatureArea::Errors, FeatureArea::Coroutines}, 4},
        {"module-ffi", {FeatureArea::Modules, FeatureArea::Ffi}, 5},
        {"metaprogramming-module", {FeatureArea::Metaprogramming, FeatureArea::Modules}, 6},
    };
}

void registerInteractionMatrixCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-124", [](TestContext& context) {
        std::string error = "stale";
        const auto cases = minimalInteractionMatrix();
        context.expect(validateInteractionMatrix(cases, error) && error.empty(),
            "TL-LANG-124 the minimal matrix must cover all six required pairs with valid records");

        auto reordered = cases;
        std::reverse(reordered.begin(), reordered.end());
        context.expect(validateInteractionMatrix(reordered, error) && error.empty(),
            "TL-LANG-124 required-pair coverage must not depend on case order");
    });
}

void registerInteractionMatrixFailureCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-125", [](TestContext& context) {
        constexpr std::string_view invalidRecord =
            "interaction cases require identity, at least two areas, and a fixed nonzero seed";
        const auto valid = minimalInteractionMatrix();
        for (std::size_t mutation = 0; mutation < 3; ++mutation)
        {
            auto cases = valid;
            if (mutation == 0) cases[0].identity.clear();
            if (mutation == 1) cases[0].areas.resize(1);
            if (mutation == 2) cases[0].deterministicSeed = 0;
            std::string error;
            context.expect(!validateInteractionMatrix(cases, error) && error == invalidRecord,
                "TL-LANG-125 each incomplete interaction record must return the first exact error");
        }

        auto duplicate = valid;
        duplicate.push_back(valid.front());
        std::string error;
        context.expect(!validateInteractionMatrix(duplicate, error) &&
                error == "duplicate feature interaction identity",
            "TL-LANG-125 duplicate identities must fail before pair-coverage validation");
    });
}

void registerPrioritizedInteractionMatrixCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-126", [](TestContext& context) {
        constexpr std::string_view invalidPriority =
            "prioritized interaction cases require high risk, compiler impact, and minimization";
        const auto valid = buildNativeSharp3InteractionMatrix(100);
        std::string error;
        context.expect(validatePrioritizedInteractionMatrix(valid, error) && error.empty(),
            "TL-LANG-126 the canonical high-risk matrix must satisfy prioritization");

        for (std::size_t mutation = 0; mutation < 3; ++mutation)
        {
            auto cases = valid;
            if (mutation == 0) cases[0].risk = InteractionRisk::Medium;
            if (mutation == 1) cases[0].minimizesOnFailure = false;
            if (mutation == 2)
            {
                cases[0].changesMir = false;
                cases[0].changesOwnership = false;
            }
            context.expect(!validatePrioritizedInteractionMatrix(cases, error) &&
                    error == invalidPriority,
                "TL-LANG-126 medium, non-minimizable, or impact-free cases must fail exactly");
        }

        const auto withoutTriple = minimalInteractionMatrix();
        context.expect(!validatePrioritizedInteractionMatrix(withoutTriple, error) &&
                error == "prioritized interaction matrix requires at least one high-risk feature triple",
            "TL-LANG-126 complete pair coverage without a triple must fail causally");
    });
}

void registerCanonicalInteractionMatrixCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-127", [](TestContext& context) {
        const std::array<std::string_view, 7> identities = {
            "variant-move-drop", "generic-trait-associated", "borrow-async-error",
            "module-ffi-contract", "derive-module-fingerprint", "closure-generic-borrow",
            "iterator-async-numeric"};
        const auto fallback = buildNativeSharp3InteractionMatrix(0);
        const auto explicitSeed = buildNativeSharp3InteractionMatrix(0x4e533300ULL);
        context.expect(fallback.size() == identities.size() && explicitSeed.size() == fallback.size(),
            "TL-LANG-127 the canonical matrix must contain exactly seven cases");
        for (std::size_t index = 0; index < fallback.size(); ++index)
        {
            context.expect(fallback[index].identity == identities[index] &&
                    fallback[index].identity == explicitSeed[index].identity &&
                    fallback[index].areas == explicitSeed[index].areas &&
                    fallback[index].deterministicSeed == 0x4e533301ULL + index &&
                    fallback[index].deterministicSeed == explicitSeed[index].deterministicSeed,
                "TL-LANG-127 zero seed fallback must preserve exact IDs, areas, and sequential seeds");
        }

        const auto wrapped = buildNativeSharp3InteractionMatrix((std::numeric_limits<std::uint64_t>::max)());
        context.expect(wrapped.size() == 7 && wrapped[0].deterministicSeed == 0 &&
                wrapped[1].deterministicSeed == 1 && wrapped[6].deterministicSeed == 6,
            "TL-LANG-127 unsigned boundary seeds must follow exact modulo arithmetic");
    });
}

void registerInteractionMinimizationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-128", [](TestContext& context) {
        const InteractionCase failing = buildNativeSharp3InteractionMatrix(1000)[2];
        const std::array essential = {FeatureArea::Coroutines, FeatureArea::Ownership};
        std::string error = "stale";
        const auto minimized = minimizeInteractionCase(failing, essential, error);
        context.expect(minimized.has_value() && error.empty() &&
                minimized->identity == "borrow-async-error-min" &&
                minimized->areas == std::vector<FeatureArea>(essential.begin(), essential.end()) &&
                minimized->deterministicSeed == failing.deterministicSeed &&
                minimized->risk == failing.risk && minimized->changesMir == failing.changesMir &&
                minimized->changesOwnership == failing.changesOwnership &&
                minimized->minimizesOnFailure == failing.minimizesOnFailure,
            "TL-LANG-128 minimization must replace only identity and areas while preserving order and fields");
    });
}

void registerInteractionMinimizationFailureCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-129", [](TestContext& context) {
        const InteractionCase failing = buildNativeSharp3InteractionMatrix(1000)[2];
        std::string error;
        const std::array<FeatureArea, 1> one = {FeatureArea::Ownership};
        context.expect(!minimizeInteractionCase(failing, one, error).has_value() &&
                error == "interaction failure requires at least two essential areas and minimization support",
            "TL-LANG-129 fewer than two essential areas must fail exactly");

        auto disabled = failing;
        disabled.minimizesOnFailure = false;
        const std::array two = {FeatureArea::Ownership, FeatureArea::Coroutines};
        context.expect(!minimizeInteractionCase(disabled, two, error).has_value() &&
                error == "interaction failure requires at least two essential areas and minimization support",
            "TL-LANG-129 disabled minimization must share the exact precondition error");

        const std::array absent = {FeatureArea::Ownership, FeatureArea::Ffi};
        context.expect(!minimizeInteractionCase(failing, absent, error).has_value() &&
                error == "minimized interaction contains an area absent from the failing case",
            "TL-LANG-129 an absent essential area must produce the exact subset error");
    });
}

void registerWholeProgramCorpusCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-130", [](TestContext& context) {
        constexpr std::string_view invalid =
            "whole-program witnesses must be unique, deterministic, incremental, and SDK-independent";
        const auto valid = buildNativeSharp3WholeProgramCorpus();
        std::string error = "stale";
        context.expect(validateWholeProgramCorpus(valid, error) && error.empty(),
            "TL-LANG-130 the canonical whole-program corpus must satisfy base invariants");
        for (std::size_t mutation = 0; mutation < 4; ++mutation)
        {
            auto programs = valid;
            if (mutation == 0) programs[0].identity.clear();
            if (mutation == 1) programs[0].deterministic = false;
            if (mutation == 2) programs[0].supportsIncremental = false;
            if (mutation == 3) programs[0].requiresSdk = true;
            context.expect(!validateWholeProgramCorpus(programs, error) && error == invalid,
                "TL-LANG-130 each broken witness invariant must return the first exact error");
        }
        auto duplicate = valid;
        duplicate[1].identity = duplicate[0].identity;
        context.expect(!validateWholeProgramCorpus(duplicate, error) && error == invalid,
            "TL-LANG-130 duplicate witness identities must return the same exact invariant error");
    });
}

void registerCanonicalWholeProgramCorpusCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-131", [](TestContext& context) {
        using Kind = WholeProgramWitness::Kind;
        const auto corpus = buildNativeSharp3WholeProgramCorpus();
        const std::array<std::string_view, 6> identities = {
            "parser", "arena", "job-graph", "serializer", "dll-host-plugin", "async-server"};
        const std::array kinds = {Kind::Parser, Kind::ArenaAllocator, Kind::JobGraph,
            Kind::BinarySerializer, Kind::DllHostPlugin, Kind::AsyncServer};
        const std::array<std::size_t, 6> units = {4, 3, 5, 4, 6, 5};
        std::string error;
        context.expect(corpus.size() == 6 && validateNativeSharp3WholeProgramCorpus(corpus, error) &&
                error.empty(),
            "TL-LANG-131 the canonical six-kind corpus must pass the extended validator");
        for (std::size_t index = 0; index < corpus.size(); ++index)
        {
            const auto& program = corpus[index];
            context.expect(program.identity == identities[index] && program.kind == kinds[index] &&
                    program.debugAndRelease && program.cleanAndIncremental && program.ltoAndNoLto &&
                    program.sanitizerReady && program.minimumSourceUnits == units[index] &&
                    program.expectedExitCode == 42 && program.partialNoHeap == (index < 4),
                "TL-LANG-131 every canonical witness must expose its exact profile and observable result");
        }

        auto missingProfile = corpus;
        missingProfile[0].debugAndRelease = false;
        context.expect(!validateNativeSharp3WholeProgramCorpus(missingProfile, error) &&
                error == "whole-program witness lacks a required build profile or observable result",
            "TL-LANG-131 a missing build profile must fail with the exact error");
        auto duplicateKind = corpus;
        duplicateKind[1].kind = duplicateKind[0].kind;
        context.expect(!validateNativeSharp3WholeProgramCorpus(duplicateKind, error) &&
                error == "whole-program corpus contains a duplicate witness kind",
            "TL-LANG-131 duplicate witness kinds must fail before complete-kind coverage");
        auto insufficientNoHeap = corpus;
        insufficientNoHeap[3].partialNoHeap = false;
        context.expect(!validateNativeSharp3WholeProgramCorpus(insufficientNoHeap, error) &&
                error == "whole-program corpus requires at least four partial noheap profiles",
            "TL-LANG-131 fewer than four partial-noheap profiles must fail exactly");
    });
}

void registerPerformanceBudgetComparisonCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-132", [](TestContext& context) {
        const PerformanceBudget maximum{10, 20, 30, 40, 50, 60, 70};
        context.expect(withinPerformanceBudget({}, maximum) &&
                withinPerformanceBudget(maximum, maximum),
            "TL-LANG-132 every enforced metric must accept below and inclusive-equal values");
        PerformanceBudget backendExcluded = maximum;
        backendExcluded.backendMilliseconds = 61;
        backendExcluded.toolchainMilliseconds = 71;
        context.expect(withinPerformanceBudget(backendExcluded, maximum),
            "TL-LANG-132 backend and toolchain metrics must remain excluded from this frontend budget");

        for (std::size_t metric = 0; metric < 5; ++metric)
        {
            PerformanceBudget measured = maximum;
            if (metric == 0) ++measured.frontendMilliseconds;
            if (metric == 1) ++measured.peakMemoryBytes;
            if (metric == 2) ++measured.mirInstructions;
            if (metric == 3) ++measured.monomorphizations;
            if (metric == 4) ++measured.codeBytes;
            context.expect(!withinPerformanceBudget(measured, maximum),
                "TL-LANG-132 each enforced metric must independently reject maximum plus one");
        }
    });
}

void registerPerformanceAssessmentCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-133", [](TestContext& context) {
        const auto profiles = nativeSharp3PerformanceBudgets();
        const std::array<std::string_view, 6> identities = {"giant-file", "generic-heavy", "macro-heavy",
            "module-fanout", "async-frames", "pathological-diagnostics"};
        const std::array corpora = {PerformanceCorpus::GiantFile, PerformanceCorpus::GenericHeavy,
            PerformanceCorpus::MacroHeavy, PerformanceCorpus::ModuleFanout,
            PerformanceCorpus::AsyncFrames, PerformanceCorpus::PathologicalDiagnostics};
        context.expect(profiles.size() == 6,
            "TL-LANG-133 the canonical performance catalog must contain six profiles");
        for (std::size_t index = 0; index < profiles.size(); ++index)
        {
            context.expect(profiles[index].identity == identities[index] &&
                    profiles[index].corpus == corpora[index] && profiles[index].tolerancePercent == 5 &&
                    profiles[index].minimumSamples == 7 &&
                    profiles[index].referenceHardware ==
                        "Windows-x64|12-logical-cores|32GiB|NVMe|clang-cl-23|NSLink-3",
                "TL-LANG-133 profile identity, corpus, tolerance, samples, and hardware must be exact");
        }

        const auto& profile = profiles.front();
        PerformanceBudget tolerated = profile.maximum;
        tolerated.frontendMilliseconds += profile.maximum.frontendMilliseconds * 5 / 100;
        tolerated.backendMilliseconds = (std::numeric_limits<std::size_t>::max)();
        tolerated.toolchainMilliseconds = (std::numeric_limits<std::size_t>::max)();
        const PerformanceAssessment pass = assessLanguagePerformance(tolerated, profile, 7);
        context.expect(pass.passed && pass.backendAndToolchainExcluded && pass.violations.empty(),
            "TL-LANG-133 inclusive tolerance must pass while backend and toolchain remain excluded");

        PerformanceBudget exceeded = tolerated;
        ++exceeded.frontendMilliseconds;
        const PerformanceAssessment slow = assessLanguagePerformance(exceeded, profile, 7);
        context.expect(!slow.passed && slow.violations ==
                std::vector<std::string>{"frontend time exceeded its language budget"},
            "TL-LANG-133 maximum tolerance plus one must produce one exact metric violation");

        auto invalid = profile;
        invalid.identity.clear();
        invalid.referenceHardware.clear();
        invalid.tolerancePercent = 26;
        const PerformanceAssessment invalidResult = assessLanguagePerformance(invalid.maximum, invalid, 6);
        context.expect(!invalidResult.passed && invalidResult.violations == std::vector<std::string>{
                "performance profile requires identity and reference hardware",
                "performance tolerance exceeds the permitted 25 percent ceiling",
                "performance sample count is below the profile minimum"},
            "TL-LANG-133 invalid profile metadata, tolerance, and sample count must preserve error order");

        PerformanceBudget allExceeded = profiles[1].maximum;
        allExceeded.frontendMilliseconds *= 2;
        allExceeded.peakMemoryBytes *= 2;
        allExceeded.mirInstructions *= 2;
        allExceeded.monomorphizations *= 2;
        allExceeded.codeBytes *= 2;
        const PerformanceAssessment violations = assessLanguagePerformance(allExceeded, profiles[1], 7);
        context.expect(!violations.passed && violations.violations == std::vector<std::string>{
                "frontend time exceeded its language budget",
                "frontend peak memory exceeded its language budget",
                "MIR size exceeded its language budget",
                "monomorphization count exceeded its language budget",
                "generated code size exceeded its language budget"},
            "TL-LANG-133 all five language budget violations must be reported in exact order");
    });
}
}

void registerLanguageIntegrationQualityModule(TestRegistry& registry)
{
    registerInteractionMatrixCase(registry);
    registerInteractionMatrixFailureCase(registry);
    registerPrioritizedInteractionMatrixCase(registry);
    registerCanonicalInteractionMatrixCase(registry);
    registerInteractionMinimizationCase(registry);
    registerInteractionMinimizationFailureCase(registry);
    registerWholeProgramCorpusCase(registry);
    registerCanonicalWholeProgramCorpusCase(registry);
    registerPerformanceBudgetComparisonCase(registry);
    registerPerformanceAssessmentCase(registry);
}
}
