#pragma once

#include <cstddef>
#include <cstdint>
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

struct InteractionCase final
{
    std::string identity;
    std::vector<FeatureArea> areas;
    std::uint64_t deterministicSeed = 0;
};
bool validateInteractionMatrix(std::span<const InteractionCase> cases, std::string& error);

struct WholeProgramWitness final
{
    std::string identity;
    bool deterministic = true;
    bool supportsIncremental = true;
    bool requiresSdk = false;
};
bool validateWholeProgramCorpus(std::span<const WholeProgramWitness> programs, std::string& error);

struct PerformanceBudget final
{
    std::size_t frontendMilliseconds = 0;
    std::size_t peakMemoryBytes = 0;
    std::size_t mirInstructions = 0;
    std::size_t monomorphizations = 0;
    std::size_t codeBytes = 0;
};
bool withinPerformanceBudget(const PerformanceBudget& measured, const PerformanceBudget& maximum) noexcept;

struct DiagnosticQuality final
{
    std::string code;
    std::string english;
    std::string spanish;
    bool precisePrimarySpan = false;
    bool fixItPreservesSemantics = false;
    std::size_t cascadeCount = 0;
};
bool diagnosticPassesQualityGate(const DiagnosticQuality& diagnostic, std::string& error);

std::string semanticDebugName(
    std::string_view owner,
    std::string_view role,
    std::size_t stableOrdinal);

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
