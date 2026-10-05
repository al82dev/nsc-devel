#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::language
{
enum class FeatureArea
{
    Aggregates,
    Variants,
    Generics,
    Traits,
    Ownership,
    Errors,
    Closures,
    Iteration,
    Coroutines,
    Metaprogramming,
    Modules,
    Ffi,
    Numerics
};
enum class InteractionRisk { Medium, High, Critical };

struct InteractionCase final
{
    std::string identity;
    std::vector<FeatureArea> areas;
    std::uint64_t deterministicSeed = 0;
    InteractionRisk risk = InteractionRisk::High;
    bool changesMir = true;
    bool changesOwnership = true;
    bool minimizesOnFailure = true;
};
bool validateInteractionMatrix(std::span<const InteractionCase> cases, std::string& error);
bool validatePrioritizedInteractionMatrix(
    std::span<const InteractionCase> cases,
    std::string& error);
std::vector<InteractionCase> buildNativeSharp3InteractionMatrix(std::uint64_t seedBase);
std::optional<InteractionCase> minimizeInteractionCase(
    const InteractionCase& failing,
    std::span<const FeatureArea> essentialAreas,
    std::string& error);

struct WholeProgramWitness final
{
    std::string identity;
    bool deterministic = true;
    bool supportsIncremental = true;
    bool requiresSdk = false;
    enum class Kind { Parser, ArenaAllocator, JobGraph, BinarySerializer, DllHostPlugin, AsyncServer };
    Kind kind = Kind::Parser;
    bool debugAndRelease = true;
    bool cleanAndIncremental = true;
    bool ltoAndNoLto = true;
    bool sanitizerReady = true;
    bool partialNoHeap = false;
    std::size_t minimumSourceUnits = 1;
    int expectedExitCode = 42;
};
bool validateWholeProgramCorpus(std::span<const WholeProgramWitness> programs, std::string& error);
std::vector<WholeProgramWitness> buildNativeSharp3WholeProgramCorpus();
bool validateNativeSharp3WholeProgramCorpus(
    std::span<const WholeProgramWitness> programs,
    std::string& error);

struct PerformanceBudget final
{
    std::size_t frontendMilliseconds = 0;
    std::size_t peakMemoryBytes = 0;
    std::size_t mirInstructions = 0;
    std::size_t monomorphizations = 0;
    std::size_t codeBytes = 0;
    std::size_t backendMilliseconds = 0;
    std::size_t toolchainMilliseconds = 0;
};
bool withinPerformanceBudget(const PerformanceBudget& measured, const PerformanceBudget& maximum) noexcept;

enum class PerformanceCorpus
{
    GiantFile,
    GenericHeavy,
    MacroHeavy,
    ModuleFanout,
    AsyncFrames,
    PathologicalDiagnostics
};

struct PerformanceBudgetProfile final
{
    std::string identity;
    PerformanceCorpus corpus = PerformanceCorpus::GiantFile;
    PerformanceBudget maximum;
    unsigned tolerancePercent = 5;
    std::string referenceHardware;
    std::size_t minimumSamples = 5;
};

struct PerformanceAssessment final
{
    bool passed = false;
    bool backendAndToolchainExcluded = true;
    std::vector<std::string> violations;
};

std::vector<PerformanceBudgetProfile> nativeSharp3PerformanceBudgets();
PerformanceAssessment assessLanguagePerformance(
    const PerformanceBudget& measured,
    const PerformanceBudgetProfile& profile,
    std::size_t sampleCount) noexcept;

enum class DiagnosticArea { Borrow, Trait, Match, Async, Module };

struct DiagnosticSpan final
{
    std::string file;
    std::size_t line = 0;
    std::size_t column = 0;
    std::size_t endLine = 0;
    std::size_t endColumn = 0;
};

struct DiagnosticFixIt final
{
    std::string replacement;
    bool preservesSemantics = false;
    bool compiles = false;
};

struct DiagnosticQuality final
{
    std::string code;
    std::string english;
    std::string spanish;
    bool precisePrimarySpan = false;
    bool fixItPreservesSemantics = false;
    std::size_t cascadeCount = 0;
    DiagnosticArea area = DiagnosticArea::Borrow;
    DiagnosticSpan primarySpan;
    std::optional<DiagnosticFixIt> fixIt;
    std::vector<std::string> macroBacktrace;
};
bool diagnosticPassesQualityGate(const DiagnosticQuality& diagnostic, std::string& error);
bool validateDiagnosticQualityCatalog(
    std::span<const DiagnosticQuality> diagnostics,
    std::size_t terminalWidth,
    std::string& error);
std::string renderDiagnosticForTerminal(
    const DiagnosticQuality& diagnostic,
    bool spanish,
    std::size_t terminalWidth);

std::string semanticDebugName(
    std::string_view owner,
    std::string_view role,
    std::size_t stableOrdinal);

enum class DebugOptimization { O0, O2 };
enum class DebugAvailability { NotYetDeclared, Live, Ended, OptimizedOut };
enum class DebugEntityKind
{
    Local,
    InlineSite,
    ClosureCapture,
    VariantPayload,
    OwnershipState,
    ExceptionValue,
    CoroutineState,
    Temporary
};

struct SemanticDebugEntry final
{
    std::string stableName;
    std::string typeName;
    DebugEntityKind kind = DebugEntityKind::Local;
    DebugOptimization optimization = DebugOptimization::O0;
    DebugAvailability availability = DebugAvailability::Live;
    std::size_t scopeBeginLine = 0;
    std::size_t scopeEndLine = 0;
    std::vector<std::size_t> sequencePointLines;
    bool sourceGenerated = false;
    bool hasInlineSite = false;
    bool valueInvented = false;
};

bool validateSemanticDebugEvidence(
    std::span<const SemanticDebugEntry> evidence,
    std::string& error);

enum class GeneratedCost { Allocation, Copy, Drop, DynamicDispatch, UnwindEdge, AtomicFence };
struct CostEvent final
{
    GeneratedCost kind;
    std::string sourceConstruct;
    bool explicitlyRequested = false;
};
bool hiddenCostAuditPasses(std::span<const CostEvent> costs, std::string& error);

std::string_view semanticToolingSchemaVersion() noexcept;
}
