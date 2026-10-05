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

struct SyntaxFreezeManifest final
{
    std::string grammarRevision;
    std::string canonicalGrammarHash;
    std::string precedenceHash;
    std::string reservedWordsHash;
    std::string recoveryRulesHash;
    bool generatedFromCanonicalSource = false;
    bool parserCorpusPassed = false;
    bool ambiguityCheckPassed = false;
    bool formatterRoundTripPassed = false;
    bool specificationSnippetsPassed = false;
};
bool validateSyntaxFreeze(const SyntaxFreezeManifest& manifest, std::string& error);

struct SemanticFreezeManifest final
{
    std::string overloadRevision;
    std::string conversionRevision;
    std::string inferenceRevision;
    std::string ownershipAndDropRevision;
    std::string errorRevision;
    std::string asyncRevision;
    std::string memoryModelRevision;
    std::vector<std::string> deferredIssueMilestones;
    bool normativeSuitePassed = false;
    bool differentialConfigurationsDeterministic = false;
};
bool validateSemanticFreeze(const SemanticFreezeManifest& manifest, std::string& error);

struct Win64AbiFreezeManifest final
{
    std::string target;
    std::string manglingRevision;
    std::string layoutRevision;
    std::string callingConventionRevision;
    std::string variantRevision;
    std::string traitObjectRevision;
    std::string errorRevision;
    std::string publicCoroutineRevision;
    std::string moduleMetadataRevision;
    bool preservedDllFixturesPassed = false;
    bool crossVersionConsumerPassed = false;
    bool symbolSnapshotsPassed = false;
    bool pdbContractPassed = false;
};
bool validateWin64AbiFreeze(const Win64AbiFreezeManifest& manifest, std::string& error);

enum class ConformanceTier { Mandatory, Recommended, ImplementationSpecific };
struct ConformanceCase final { std::string identity; ConformanceTier tier; };
bool validateConformanceManifest(std::span<const ConformanceCase> cases, std::string& error);

struct CandidateConformanceManifest final
{
    std::string suiteVersion;
    std::string resultSchemaVersion;
    std::vector<ConformanceCase> cases;
    bool selfValidationPassed = false;
    bool brokenCompilerRejected = false;
    bool shardRetryEquivalent = false;
    bool hermeticRunPassed = false;
};
bool validateCandidateConformance(
    const CandidateConformanceManifest& manifest,
    std::string& error);

struct BilingualClause final
{
    std::string identity;
    std::string spanishRequirement;
    std::string englishRequirement;
    std::size_t spanishExamples = 0;
    std::size_t englishExamples = 0;
    std::size_t spanishCompatibilityNotes = 0;
    std::size_t englishCompatibilityNotes = 0;
    bool glossaryConsistent = true;
    bool linksValid = true;
    bool codeSamplesCompile = true;
    bool presentInFinalIndexes = true;
};
bool bilingualSpecificationInParity(std::span<const BilingualClause> clauses, std::string& error);
bool validateBilingualSpecificationAudit(
    std::span<const BilingualClause> clauses,
    std::string& error);

enum class ReleaseCandidateStage { RC1, RC2, RC3 };
enum class ChangeSeverity { Feature, Fix, PromotionBlocker };
bool changeAllowed(ReleaseCandidateStage stage, ChangeSeverity severity) noexcept;

struct ReleaseCandidateEvidence final
{
    ReleaseCandidateStage stage = ReleaseCandidateStage::RC1;
    std::string identity;
    std::string featureSetHash;
    std::string knownIssuesHash;
    std::vector<ChangeSeverity> acceptedChanges;
    bool affectedGatesRestarted = false;
    bool cleanMatrixPassed = false;
    bool metadataUpgradeDowngradePassed = false;
    bool reproducible = false;
    bool longRunningStressPassed = false;
    bool packageFreeSamplesPassed = false;
};
bool validateReleaseCandidate(
    const ReleaseCandidateEvidence& evidence,
    std::string& error);

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
