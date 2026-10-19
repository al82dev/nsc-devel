#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "nativesharp/Language/BilingualClause.h"
#include "nativesharp/Language/CandidateConformanceManifest.h"
#include "nativesharp/Language/ChangeSeverity.h"
#include "nativesharp/Language/ConformanceCase.h"
#include "nativesharp/Language/ConformanceTier.h"
#include "nativesharp/Language/FreezeManifest.h"
#include "nativesharp/Language/OfficialPromotionManifest.h"
#include "nativesharp/Language/PromotionEvidence.h"
#include "nativesharp/Language/PromotionStatus.h"
#include "nativesharp/Language/ReleaseCandidateEvidence.h"
#include "nativesharp/Language/ReleaseCandidateStage.h"
#include "nativesharp/Language/SemanticFreezeManifest.h"
#include "nativesharp/Language/SyntaxFreezeManifest.h"
#include "nativesharp/Language/Win64AbiFreezeManifest.h"

namespace nativesharp::language
{
bool validateFreezeManifest(const FreezeManifest& manifest, std::string& error);

bool validateSyntaxFreeze(const SyntaxFreezeManifest& manifest, std::string& error);

bool validateSemanticFreeze(const SemanticFreezeManifest& manifest, std::string& error);

bool validateWin64AbiFreeze(const Win64AbiFreezeManifest& manifest, std::string& error);

bool validateConformanceManifest(std::span<const ConformanceCase> cases, std::string& error);

bool validateCandidateConformance(
    const CandidateConformanceManifest& manifest,
    std::string& error);

bool bilingualSpecificationInParity(std::span<const BilingualClause> clauses, std::string& error);
bool validateBilingualSpecificationAudit(
    std::span<const BilingualClause> clauses,
    std::string& error);

bool changeAllowed(ReleaseCandidateStage stage, ChangeSeverity severity) noexcept;

bool validateReleaseCandidate(
    const ReleaseCandidateEvidence& evidence,
    std::string& error);

PromotionStatus promotionStatus(const PromotionEvidence& evidence) noexcept;
std::vector<std::string> missingPromotionEvidence(const PromotionEvidence& evidence);

PromotionStatus officialPromotionStatus(
    const OfficialPromotionManifest& manifest,
    std::vector<std::string>& missing);
std::string_view stableLanguageVersion() noexcept;
std::string_view normativeTarget() noexcept;
}
