#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "nativesharp/Language/CostAuditRecord.h"
#include "nativesharp/Language/CostEvent.h"
#include "nativesharp/Language/CostOptimization.h"
#include "nativesharp/Language/DebugAvailability.h"
#include "nativesharp/Language/DebugEntityKind.h"
#include "nativesharp/Language/DebugOptimization.h"
#include "nativesharp/Language/DiagnosticArea.h"
#include "nativesharp/Language/DiagnosticFixIt.h"
#include "nativesharp/Language/DiagnosticQuality.h"
#include "nativesharp/Language/DiagnosticSpan.h"
#include "nativesharp/Language/FeatureArea.h"
#include "nativesharp/Language/GeneratedCost.h"
#include "nativesharp/Language/InteractionCase.h"
#include "nativesharp/Language/InteractionRisk.h"
#include "nativesharp/Language/PerformanceAssessment.h"
#include "nativesharp/Language/PerformanceBudget.h"
#include "nativesharp/Language/PerformanceBudgetProfile.h"
#include "nativesharp/Language/PerformanceCorpus.h"
#include "nativesharp/Language/SemanticDebugEntry.h"
#include "nativesharp/Language/WholeProgramWitness.h"

namespace nativesharp::language
{
bool validateInteractionMatrix(std::span<const InteractionCase> cases, std::string& error);
bool validatePrioritizedInteractionMatrix(
    std::span<const InteractionCase> cases,
    std::string& error);
std::vector<InteractionCase> buildNativeSharp3InteractionMatrix(std::uint64_t seedBase);
std::optional<InteractionCase> minimizeInteractionCase(
    const InteractionCase& failing,
    std::span<const FeatureArea> essentialAreas,
    std::string& error);

bool validateWholeProgramCorpus(std::span<const WholeProgramWitness> programs, std::string& error);
std::vector<WholeProgramWitness> buildNativeSharp3WholeProgramCorpus();
bool validateNativeSharp3WholeProgramCorpus(
    std::span<const WholeProgramWitness> programs,
    std::string& error);

bool withinPerformanceBudget(const PerformanceBudget& measured, const PerformanceBudget& maximum) noexcept;

std::vector<PerformanceBudgetProfile> nativeSharp3PerformanceBudgets();
PerformanceAssessment assessLanguagePerformance(
    const PerformanceBudget& measured,
    const PerformanceBudgetProfile& profile,
    std::size_t sampleCount) noexcept;

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

bool validateSemanticDebugEvidence(
    std::span<const SemanticDebugEntry> evidence,
    std::string& error);

bool hiddenCostAuditPasses(std::span<const CostEvent> costs, std::string& error);
bool validateZeroCostAudit(
    std::span<const CostAuditRecord> records,
    std::string& error);

std::string_view semanticToolingSchemaVersion() noexcept;
}
