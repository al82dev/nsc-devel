#include "nativesharp/Frontend/GenericTelemetry.h"
#include <algorithm>
#include <sstream>
#include <vector>

namespace nativesharp::frontend
{
void GenericTelemetry::recordSubPhase(std::string_view name, std::chrono::nanoseconds elapsed)
    {
        subPhaseMicros[std::string(name)] += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
        if (timingSink) timingSink(name, elapsed);
    }

void GenericTelemetry::noteResolutionQuery(std::size_t candidates)
    {
        if (!enabled) return;
        ++resolutionQueries;
        candidatesExamined += candidates;
    }

void GenericTelemetry::noteTemplate(std::size_t kindIndex)
    {
        if (!enabled) return;
        ++templatesCollected;
        if (kindIndex < 5U) ++templatesByKind[kindIndex];
    }

void GenericTelemetry::noteScopeCopy(std::size_t entries)
    {
        if (!enabled) return;
        ++scopeCopies;
        scopeCopiedEntries += entries;
    }

void GenericTelemetry::noteSpecializationHit()
    {
        if (enabled) ++specializationHits;
    }

void GenericTelemetry::noteMaterialization(
        std::string_view canonicalKey, std::chrono::nanoseconds elapsed)
    {
        if (!enabled) return;
        ++demandsMaterialized;
        if (detailed)
        {
            TemplateDetail& detail = templates[std::string(canonicalKey)];
            ++detail.materializations;
            detail.exclusiveMicros += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
        }
    }

void GenericTelemetry::noteDemand(bool duplicated)
    {
        if (!enabled) return;
        ++demandsDiscovered;
        if (duplicated) ++demandsDuplicated;
    }

void GenericTelemetry::noteNodesCloned()
    {
        if (enabled) ++nodesCloned;
    }

void GenericTelemetry::noteTypesCloned()
    {
        if (enabled) ++typesCloned;
    }

#if 0 // INSTRUMENTATION CODE: progress-aware scope retained for diagnostic builds.
GenericTelemetry::Scope::Scope(GenericTelemetry& telemetry, std::string_view name)
            : telemetry_(&telemetry), name_(name),
              started_(telemetry.enabled || telemetry.progressSink
                  ? std::chrono::steady_clock::now()
                  : std::chrono::steady_clock::time_point{})
        {
            telemetry_->reportProgress(
                basic::CompilationEventKind::PhaseStarted, name_);
        }
GenericTelemetry::Scope::~Scope()
        {
            if (!telemetry_->enabled && !telemetry_->progressSink) return;
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started_);
            if (telemetry_->enabled) telemetry_->recordSubPhase(name_, elapsed);
            telemetry_->reportProgress(
                basic::CompilationEventKind::PhaseFinished, name_, {}, 0, 0, elapsed);
        }
#else
GenericTelemetry::Scope::Scope(GenericTelemetry& telemetry, std::string_view name)
            : telemetry_(&telemetry), name_(name),
              started_(telemetry.enabled ? std::chrono::steady_clock::now()
                                         : std::chrono::steady_clock::time_point{})
        {
        }
GenericTelemetry::Scope::~Scope()
        {
            if (!telemetry_->enabled) return;
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started_);
            telemetry_->recordSubPhase(name_, elapsed);
        }
#endif // INSTRUMENTATION CODE
std::string GenericTelemetry::renderSummary() const
{
    // Fixed order keeps the summary deterministic regardless of unordered_map iteration.
    static constexpr std::string_view subPhaseNames[] = {
        "generics.visibility",
        "generics.alias-expansion",
        "generics.trait-merge",
        "generics.opaque-results",
        "generics.ordinary-return-index",
        "generics.associated-items",
        "generics.template-collection",
        "generics.template-index",
        "generics.dependency-discovery",
        "generics.imported-specialization-seed",
        "generics.root-demand-discovery",
        "generics.specialization-waves",
        "generics.type-demand-waves",
        "generics.rewrite-commit",
        "generics.body-type-demands",
    };
    std::ostringstream output;
    output << "generics.telemetry enabled=" << (enabled ? 1 : 0)
           << " detailed=" << (detailed ? 1 : 0)
           << " own-units=" << ownUnits
           << " template-source-bytes=" << templateSourceBytes
           << " templates=" << templatesCollected
           << " resolution-queries=" << resolutionQueries
           << " candidates-examined=" << candidatesExamined
           << " demands-discovered=" << demandsDiscovered
           << " demands-duplicated=" << demandsDuplicated
           << " demands-materialized=" << demandsMaterialized
           << " specialization-hits=" << specializationHits
           << " nodes-cloned=" << nodesCloned
           << " types-cloned=" << typesCloned
           << " maximum-logical-depth=" << maximumLogicalDepth
           << " scope-copies=" << scopeCopies
           << " scope-copied-entries=" << scopeCopiedEntries << '\n';
    for (const std::string_view name : subPhaseNames)
    {
        const auto found = subPhaseMicros.find(std::string(name));
        output << "subphase " << name << " us="
               << (found == subPhaseMicros.end() ? 0U : found->second) << '\n';
    }
    // Per-template detail uses canonical-key order so the report is reproducible.
    std::vector<std::pair<std::string, TemplateDetail>> sortedTemplates(
        templates.begin(), templates.end());
    std::sort(sortedTemplates.begin(), sortedTemplates.end(),
        [](const auto& left, const auto& right) { return left.first < right.first; });
    for (const auto& [key, detail] : sortedTemplates)
    {
        output << "template-detail key=" << key
               << " materializations=" << detail.materializations
               << " exclusive_us=" << detail.exclusiveMicros << '\n';
    }
    return output.str();
}
}
