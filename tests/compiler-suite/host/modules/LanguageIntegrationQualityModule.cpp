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

DiagnosticQuality makeDiagnostic(std::string code, DiagnosticArea area)
{
    DiagnosticQuality result;
    result.code = std::move(code);
    result.english = "the source construct violates its semantic contract";
    result.spanish = "la construccion fuente viola su contrato semantico";
    result.precisePrimarySpan = true;
    result.fixItPreservesSemantics = true;
    result.cascadeCount = 1;
    result.area = area;
    result.primarySpan = {"Main.ns", 4, 9, 4, 17};
    result.fixIt = DiagnosticFixIt{"safe_replacement", true, true};
    result.macroBacktrace = {"outer! at Main.ns:2", "inner! at Main.ns:4"};
    return result;
}

std::array<DiagnosticQuality, 5> diagnosticCatalog()
{
    return {
        makeDiagnostic("NS3B001", DiagnosticArea::Borrow),
        makeDiagnostic("NS3T001", DiagnosticArea::Trait),
        makeDiagnostic("NS3M001", DiagnosticArea::Match),
        makeDiagnostic("NS3A001", DiagnosticArea::Async),
        makeDiagnostic("NS3D001", DiagnosticArea::Module),
    };
}

void registerDiagnosticQualityGateCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-134", [](TestContext& context) {
        constexpr std::string_view general =
            "diagnostic quality gate requires stable code, localization, precise span, safe fix-it, and "
            "bounded cascades";
        const DiagnosticQuality valid = makeDiagnostic("NS3B001", DiagnosticArea::Borrow);
        std::string error = "stale";
        context.expect(diagnosticPassesQualityGate(valid, error) && error.empty(),
            "TL-LANG-134 a complete localized diagnostic must pass the quality gate");

        for (std::size_t mutation = 0; mutation < 6; ++mutation)
        {
            DiagnosticQuality diagnostic = valid;
            if (mutation == 0) diagnostic.code = "XX3B001";
            if (mutation == 1) diagnostic.english.clear();
            if (mutation == 2) diagnostic.spanish.clear();
            if (mutation == 3) diagnostic.precisePrimarySpan = false;
            if (mutation == 4) diagnostic.fixItPreservesSemantics = false;
            if (mutation == 5) diagnostic.cascadeCount = 3;
            context.expect(!diagnosticPassesQualityGate(diagnostic, error) && error == general,
                "TL-LANG-134 every top-level quality invariant must return the shared exact error");
        }

        for (std::size_t mutation = 0; mutation < 4; ++mutation)
        {
            DiagnosticQuality diagnostic = valid;
            if (mutation == 0) diagnostic.primarySpan.line = 0;
            if (mutation == 1) diagnostic.primarySpan.column = 0;
            if (mutation == 2) diagnostic.primarySpan.endLine = 3;
            if (mutation == 3) diagnostic.primarySpan.endColumn = 8;
            context.expect(!diagnosticPassesQualityGate(diagnostic, error) &&
                    error == "diagnostic primary span is invalid",
                "TL-LANG-134 each invalid physical span boundary must return the exact span error");
        }

        for (std::size_t mutation = 0; mutation < 3; ++mutation)
        {
            DiagnosticQuality diagnostic = valid;
            if (mutation == 0) diagnostic.fixIt->replacement.clear();
            if (mutation == 1) diagnostic.fixIt->preservesSemantics = false;
            if (mutation == 2) diagnostic.fixIt->compiles = false;
            context.expect(!diagnosticPassesQualityGate(diagnostic, error) &&
                    error == "diagnostic fix-it must be nonempty, semantics-preserving, and compilable",
                "TL-LANG-134 every invalid fix-it field must return the exact fix-it error");
        }

        DiagnosticQuality verbose = valid;
        verbose.macroBacktrace.resize(5, "frame");
        context.expect(!diagnosticPassesQualityGate(verbose, error) &&
                error == "diagnostic macro backtrace is not compact",
            "TL-LANG-134 a fifth macro frame must return the exact compactness error");
    });
}

void registerDiagnosticCatalogCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-135", [](TestContext& context) {
        const auto valid = diagnosticCatalog();
        std::string error = "stale";
        context.expect(validateDiagnosticQualityCatalog(valid, 80, error) && error.empty(),
            "TL-LANG-135 five distinct diagnostic areas must pass at a readable width");
        context.expect(!validateDiagnosticQualityCatalog(valid, 39, error) &&
                error == "diagnostic terminal width must be at least 40 columns",
            "TL-LANG-135 width 39 must fail before catalog traversal");

        auto noPhysicalSpan = valid;
        noPhysicalSpan[0].primarySpan.file.clear();
        context.expect(!validateDiagnosticQualityCatalog(noPhysicalSpan, 80, error) &&
                error == "diagnostic catalog entries require a physical primary span",
            "TL-LANG-135 catalog entries must require a physical primary span");

        auto duplicate = valid;
        duplicate[4].code = duplicate[0].code;
        context.expect(!validateDiagnosticQualityCatalog(duplicate, 80, error) &&
                error == "diagnostic catalog contains a duplicate stable code",
            "TL-LANG-135 duplicate stable codes must fail exactly");

        auto missingArea = valid;
        missingArea[4].area = DiagnosticArea::Borrow;
        context.expect(!validateDiagnosticQualityCatalog(missingArea, 80, error) &&
                error == "diagnostic catalog must cover borrow, trait, match, async, and module errors",
            "TL-LANG-135 a missing diagnostic area must fail the final coverage check");
    });
}

void registerDiagnosticRenderingCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-136", [](TestContext& context) {
        DiagnosticQuality diagnostic = makeDiagnostic("NS3Q001", DiagnosticArea::Module);
        diagnostic.english = "abcdef";
        diagnostic.spanish = "uvwxyz";
        context.expect(renderDiagnosticForTerminal(diagnostic, false, 100) == "NS3Q001: abcdef" &&
                renderDiagnosticForTerminal(diagnostic, true, 100) == "NS3Q001: uvwxyz",
            "TL-LANG-136 English and Spanish full renderings must preserve exact bytes");
        context.expect(renderDiagnosticForTerminal(diagnostic, false, 0).empty() &&
                renderDiagnosticForTerminal(diagnostic, false, 1) == "N" &&
                renderDiagnosticForTerminal(diagnostic, false, 3) == "NS3",
            "TL-LANG-136 widths zero through three must use exact prefix truncation without ellipsis");
        context.expect(renderDiagnosticForTerminal(diagnostic, false, 12) == "NS3Q001: ..." &&
                renderDiagnosticForTerminal(diagnostic, true, 12) == "NS3Q001: ...",
            "TL-LANG-136 wider truncation must reserve exactly three bytes for an ellipsis");
    });
}

void registerSemanticDebugNameCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-137", [](TestContext& context) {
        context.expect(semanticDebugName("Owner.Type", "async state#1", 42) ==
                "Owner.Type$async_state_1$42",
            "TL-LANG-137 role punctuation must normalize to underscores while owner remains exact");
        context.expect(semanticDebugName("", "", 0) == "$$0" &&
                semanticDebugName("A", "Az09_-", 7) == "A$Az09__$7",
            "TL-LANG-137 empty, alphanumeric, and punctuation boundaries must be stable");
    });
}

SemanticDebugEntry makeDebugEntry(
    std::string role,
    DebugEntityKind kind,
    DebugOptimization optimization)
{
    SemanticDebugEntry result;
    result.stableName = semanticDebugName("Main", role, static_cast<std::size_t>(kind));
    result.typeName = kind == DebugEntityKind::VariantPayload ? "Result::Ok(i32)" : "i32";
    result.kind = kind;
    result.optimization = optimization;
    result.scopeBeginLine = 10;
    result.scopeEndLine = 20;
    result.sequencePointLines = {10, 15, 20};
    result.sourceGenerated = kind == DebugEntityKind::CoroutineState || kind == DebugEntityKind::Temporary;
    result.hasInlineSite = kind == DebugEntityKind::InlineSite;
    return result;
}

std::array<SemanticDebugEntry, 8> completeDebugEvidence()
{
    return {
        makeDebugEntry("local", DebugEntityKind::Local, DebugOptimization::O0),
        makeDebugEntry("inline", DebugEntityKind::InlineSite, DebugOptimization::O0),
        makeDebugEntry("capture", DebugEntityKind::ClosureCapture, DebugOptimization::O0),
        makeDebugEntry("variant", DebugEntityKind::VariantPayload, DebugOptimization::O0),
        makeDebugEntry("owner", DebugEntityKind::OwnershipState, DebugOptimization::O0),
        makeDebugEntry("exception", DebugEntityKind::ExceptionValue, DebugOptimization::O0),
        makeDebugEntry("coroutine", DebugEntityKind::CoroutineState, DebugOptimization::O0),
        makeDebugEntry("temporary", DebugEntityKind::Temporary, DebugOptimization::O2),
    };
}

void registerSemanticDebugEvidenceCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-138", [](TestContext& context) {
        const auto valid = completeDebugEvidence();
        std::string error = "stale";
        context.expect(validateSemanticDebugEvidence(valid, error) && error.empty(),
            "TL-LANG-138 complete O0/O2 evidence for all eight kinds must pass");

        for (std::size_t mutation = 0; mutation < 3; ++mutation)
        {
            auto evidence = valid;
            if (mutation == 0) evidence[0].stableName.clear();
            if (mutation == 1) evidence[0].typeName.clear();
            if (mutation == 2) evidence[1].stableName = evidence[0].stableName;
            context.expect(!validateSemanticDebugEvidence(evidence, error) &&
                    error == "semantic debug evidence requires stable unique names and concrete types",
                "TL-LANG-138 empty identity/type and duplicate identity per optimization must fail exactly");
        }

        auto invalidLifetime = valid;
        invalidLifetime[0].scopeBeginLine = 0;
        context.expect(!validateSemanticDebugEvidence(invalidLifetime, error) &&
                error == "semantic debug evidence contains an invalid source lifetime",
            "TL-LANG-138 a zero source lifetime must fail exactly");
        auto invented = valid;
        invented[7].valueInvented = true;
        context.expect(!validateSemanticDebugEvidence(invented, error) &&
                error == "optimized debug evidence must never invent a value",
            "TL-LANG-138 invented optimized values must fail exactly");

        for (std::size_t mutation = 0; mutation < 2; ++mutation)
        {
            auto evidence = valid;
            if (mutation == 0) evidence[0].availability = DebugAvailability::OptimizedOut;
            if (mutation == 1) evidence[0].sequencePointLines.clear();
            context.expect(!validateSemanticDebugEvidence(evidence, error) &&
                    error == "O0 debug evidence requires source fidelity and sequence points",
                "TL-LANG-138 optimized-out or point-free O0 evidence must fail exactly");
        }

        auto outside = valid;
        outside[2].sequencePointLines = {9};
        context.expect(!validateSemanticDebugEvidence(outside, error) &&
                error == "semantic debug sequence point lies outside its source lifetime",
            "TL-LANG-138 sequence points outside the lifetime must fail exactly");
        auto noInline = valid;
        noInline[1].hasInlineSite = false;
        context.expect(!validateSemanticDebugEvidence(noInline, error) &&
                error == "inline debug evidence requires an inline-site record",
            "TL-LANG-138 inline entities without inline-site records must fail exactly");
        auto generatedLocal = valid;
        generatedLocal[0].sourceGenerated = true;
        context.expect(!validateSemanticDebugEvidence(generatedLocal, error) &&
                error == "generated compiler state must not be presented as a source local",
            "TL-LANG-138 generated compiler state must not masquerade as a local");

        auto noO2 = valid;
        noO2[7].optimization = DebugOptimization::O0;
        auto noO0 = valid;
        for (auto& entry : noO0) entry.optimization = DebugOptimization::O2;
        std::vector<SemanticDebugEntry> missingKind(valid.begin() + 1, valid.end());
        for (const auto& evidence : {std::vector<SemanticDebugEntry>(noO2.begin(), noO2.end()),
                 std::vector<SemanticDebugEntry>(noO0.begin(), noO0.end()), missingKind})
        {
            context.expect(!validateSemanticDebugEvidence(evidence, error) &&
                    error == "semantic debug audit requires O0/O2 and every observable entity category",
                "TL-LANG-138 missing optimization modes or entity kinds must fail the final audit");
        }
    });
}

void registerHiddenCostAuditCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-139", [](TestContext& context) {
        const std::array<CostEvent, 0> empty{};
        const std::array explicitCosts = {
            CostEvent{GeneratedCost::Copy, "copy", true},
            CostEvent{GeneratedCost::Drop, "drop", true},
        };
        std::string error = "stale";
        context.expect(hiddenCostAuditPasses(empty, error) && error.empty() &&
                hiddenCostAuditPasses(explicitCosts, error) && error.empty(),
            "TL-LANG-139 empty and fully explicit cost sets must pass and clear stale errors");
        const std::array hidden = {
            CostEvent{GeneratedCost::Allocation, "implicit-box", false},
            CostEvent{GeneratedCost::Copy, "later-copy", false},
        };
        context.expect(!hiddenCostAuditPasses(hidden, error) &&
                error == "hidden generated cost in implicit-box",
            "TL-LANG-139 the first hidden cost must fail with its exact source construct");
    });
}

CostAuditRecord costRecord(
    GeneratedCost kind,
    std::string construct,
    CostOptimization optimization)
{
    return {{kind, std::move(construct), true}, optimization, true, 1, 1, {}};
}

std::array<CostAuditRecord, 6> completeCostAudit()
{
    return {
        costRecord(GeneratedCost::Allocation, "owned-array", CostOptimization::Off),
        costRecord(GeneratedCost::Copy, "copy-value", CostOptimization::Off),
        costRecord(GeneratedCost::Drop, "owned-scope", CostOptimization::Off),
        costRecord(GeneratedCost::DynamicDispatch, "dyn-trait-call", CostOptimization::On),
        costRecord(GeneratedCost::UnwindEdge, "typed-throws", CostOptimization::On),
        costRecord(GeneratedCost::AtomicFence, "atomic-release", CostOptimization::On),
    };
}

void registerZeroCostAuditCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-140", [](TestContext& context) {
        const auto valid = completeCostAudit();
        std::string error = "stale";
        context.expect(validateZeroCostAudit(valid, error) && error.empty(),
            "TL-LANG-140 six costs across Off/On with MIR and assembly evidence must pass");

        auto unnamed = valid;
        unnamed[0].event.sourceConstruct.clear();
        context.expect(!validateZeroCostAudit(unnamed, error) &&
                error == "zero-cost audit requires named library-free source fixtures",
            "TL-LANG-140 unnamed fixtures must fail the first exact invariant");
        auto linked = valid;
        linked[0].libraryFree = false;
        context.expect(!validateZeroCostAudit(linked, error) &&
                error == "zero-cost audit requires named library-free source fixtures",
            "TL-LANG-140 library-dependent fixtures must fail the first exact invariant");

        auto duplicate = valid;
        duplicate[1].event.sourceConstruct = duplicate[0].event.sourceConstruct;
        context.expect(!validateZeroCostAudit(duplicate, error) &&
                error == "zero-cost audit contains duplicate construct/optimization evidence",
            "TL-LANG-140 duplicate construct/optimization evidence must fail exactly");

        for (std::size_t mutation = 0; mutation < 2; ++mutation)
        {
            auto records = valid;
            if (mutation == 0) records[0].mirOccurrences = 0;
            if (mutation == 1) records[0].assemblyOccurrences = 0;
            context.expect(!validateZeroCostAudit(records, error) &&
                    error == "zero-cost audit requires MIR and assembly evidence",
                "TL-LANG-140 absent MIR or assembly occurrences must fail exactly");
        }

        auto hidden = valid;
        hidden[0].event.explicitlyRequested = false;
        context.expect(!validateZeroCostAudit(hidden, error) &&
                error == "generated cost is hidden and has no approved exception",
            "TL-LANG-140 hidden costs without an exception must fail exactly");
        hidden[0].approvedException = "ABI-mandated ownership box";
        context.expect(validateZeroCostAudit(hidden, error) && error.empty(),
            "TL-LANG-140 a documented exception must permit an otherwise complete hidden cost");
        auto mislabeled = valid;
        mislabeled[0].approvedException = "not required";
        context.expect(!validateZeroCostAudit(mislabeled, error) &&
                error == "explicit costs must not be mislabeled as exceptions",
            "TL-LANG-140 explicit costs with exceptions must fail exactly");

        std::vector<CostAuditRecord> missingKind(valid.begin() + 1, valid.end());
        context.expect(!validateZeroCostAudit(missingKind, error) &&
                error == "zero-cost audit requires all six cost kinds with optimization off and on",
            "TL-LANG-140 a missing cost kind must fail final coverage");
        auto oneMode = valid;
        for (auto& record : oneMode) record.optimization = CostOptimization::Off;
        context.expect(!validateZeroCostAudit(oneMode, error) &&
                error == "zero-cost audit requires all six cost kinds with optimization off and on",
            "TL-LANG-140 a missing optimization mode must fail final coverage");
    });
}

void registerSemanticToolingSchemaCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-141", [](TestContext& context) {
        const std::string_view first = semanticToolingSchemaVersion();
        const std::string_view second = semanticToolingSchemaVersion();
        context.expect(first == "nsc-semantic-tooling-v1" && second == first &&
                first.data() == second.data(),
            "TL-LANG-141 the semantic tooling schema bytes and stable storage must remain exact");
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
    registerDiagnosticQualityGateCase(registry);
    registerDiagnosticCatalogCase(registry);
    registerDiagnosticRenderingCase(registry);
    registerSemanticDebugNameCase(registry);
    registerSemanticDebugEvidenceCase(registry);
    registerHiddenCostAuditCase(registry);
    registerZeroCostAuditCase(registry);
    registerSemanticToolingSchemaCase(registry);
}
}
