#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::language
{
struct FreezeManifest final
{
    std::string syntaxRevision;
    std::string semanticRevision;
    std::string win64AbiRevision;
    std::string metadataRevision;
};
bool validateFreezeManifest(const FreezeManifest& manifest, std::string& error);

enum class ConformanceTier { Mandatory, Recommended, ImplementationSpecific };
struct ConformanceCase final { std::string identity; ConformanceTier tier; };
bool validateConformanceManifest(std::span<const ConformanceCase> cases, std::string& error);

struct BilingualClause final
{
    std::string identity;
    std::string spanishRequirement;
    std::string englishRequirement;
    std::size_t spanishExamples = 0;
    std::size_t englishExamples = 0;
};
bool bilingualSpecificationInParity(std::span<const BilingualClause> clauses, std::string& error);

enum class ReleaseCandidateStage { RC1, RC2, RC3 };
enum class ChangeSeverity { Feature, Fix, PromotionBlocker };
bool changeAllowed(ReleaseCandidateStage stage, ChangeSeverity severity) noexcept;

struct PromotionEvidence final
{
    bool externalBaselinePassed = false;
    std::size_t priorityZeroIssues = 0;
    std::size_t unwaivedPriorityOneIssues = 0;
    bool debugConformancePassed = false;
    bool releaseConformancePassed = false;
    bool bilingualDocsInParity = false;
    bool migrationValidated = false;
    bool artifactsReproducible = false;
    bool hashesPublished = false;
};
enum class PromotionStatus { NotReady, Ready };
PromotionStatus promotionStatus(const PromotionEvidence& evidence) noexcept;
std::vector<std::string> missingPromotionEvidence(const PromotionEvidence& evidence);
std::string_view stableLanguageVersion() noexcept;
std::string_view normativeTarget() noexcept;
}
