#pragma once

#if 0 // INSTRUMENTATION CODE: detailed generics trace support.
#include "nativesharp/Basic/CompilationEvent.h"
#endif // INSTRUMENTATION CODE

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace nativesharp::frontend
{
// NSC305-P01. Observability-only accumulator for the generic monomorphization phase.
//
// It records hierarchical sub-phase timings and aggregate counters so a `--time` /
// `--time-trace` run can attribute the `generics` phase to its internal steps and, with
// detail enabled, to individual templates. Recording is a no-op unless `enabled` is true;
// the per-template breakdown is recorded only when `detailed` is also true. This struct must
// never influence diagnostics, artifacts, control flow, ordering or scheduling: every
// instrumented site only appends to these fields or forwards a timing to `timingSink`.
struct GenericTelemetry final
{
    using TimingSink = std::function<void(std::string_view, std::chrono::nanoseconds)>;
#if 0 // INSTRUMENTATION CODE
    using ProgressSink = std::function<void(const basic::CompilationEvent&)>;
#endif // INSTRUMENTATION CODE

    bool enabled = false;
    bool detailed = false;
    TimingSink timingSink;
#if 0 // INSTRUMENTATION CODE
    // Live, crash-surviving breadcrumbs are independent from timing collection. The driver sink
    // serializes concurrent worker events and flushes every rendered line before returning.
    ProgressSink progressSink;
#endif // INSTRUMENTATION CODE

    // Sub-phase wall times accumulated by name. A sub-phase may span more than one lexical
    // scope (for example `generics.template-index`); times for the same name are summed.
    std::unordered_map<std::string, std::uint64_t> subPhaseMicros;

    std::uint64_t ownUnits = 0;
    std::uint64_t templateSourceBytes = 0;
    std::uint64_t templatesCollected = 0;
    // Indexed by GenericTemplateKind (Class, Struct, Delegate, Method, Interface).
    std::uint64_t templatesByKind[5] = {};
    std::uint64_t resolutionQueries = 0;
    std::uint64_t candidatesExamined = 0;
    std::uint64_t demandsDiscovered = 0;
    std::uint64_t demandsDuplicated = 0;
    std::uint64_t demandsMaterialized = 0;
    std::uint64_t specializationHits = 0;
    // NSC305-G09. Cloned on the worker thread (outside the wave mutex), so these two counters are
    // atomic; the remaining counters are written only on the serial coordinator.
    std::atomic<std::uint64_t> nodesCloned{0};
    std::atomic<std::uint64_t> typesCloned{0};
    std::uint64_t maximumLogicalDepth = 0;
    // NSC305-G04. Scope-copy cardinality: how many `GenericLookupScope` deep copies were made and
    // the total container entries those copies moved. A large `scopeCopiedEntries` per copy is the
    // O(N^2) signal that motivates persistent scopes.
    std::uint64_t scopeCopies = 0;
    std::uint64_t scopeCopiedEntries = 0;

    struct TemplateDetail final
    {
        std::uint64_t materializations = 0;
        std::uint64_t exclusiveMicros = 0;
    };
    // Keyed by the template canonical key; populated only while `detailed` is enabled.
    std::unordered_map<std::string, TemplateDetail> templates;

    void recordSubPhase(std::string_view name, std::chrono::nanoseconds elapsed);

#if 0 // INSTRUMENTATION CODE
    void reportProgress(
        basic::CompilationEventKind kind,
        std::string_view phase,
        std::string_view detail = {},
        std::size_t current = 0,
        std::size_t total = 0,
        std::chrono::nanoseconds duration = {}) const
    {
        if (progressSink)
            progressSink({kind, phase, detail, current, total, duration});
    }
#endif // INSTRUMENTATION CODE

    void noteResolutionQuery(std::size_t candidates);

    void noteTemplate(std::size_t kindIndex);

    void noteScopeCopy(std::size_t entries);

    void noteSpecializationHit();

    void noteMaterialization(
        std::string_view canonicalKey, std::chrono::nanoseconds elapsed);

    void noteDemand(bool duplicated);

    void noteNodesCloned();

    void noteTypesCloned();

    class Scope final
    {
    public:
        Scope(GenericTelemetry& telemetry, std::string_view name);
        ~Scope();
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        GenericTelemetry* telemetry_;
        std::string_view name_;
        std::chrono::steady_clock::time_point started_;
    };

    // Deterministic, human-readable summary of counters and per-template detail. Implemented
    // in ProjectGenericModel.cpp to keep this header free of <sstream>.
    std::string renderSummary() const;
};
}
