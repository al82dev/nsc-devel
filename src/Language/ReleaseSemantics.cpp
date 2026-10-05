#include "nativesharp/Language/ReleaseSemantics.h"

#include <unordered_set>

namespace nativesharp::language
{
bool validateFreezeManifest(const FreezeManifest& manifest, std::string& error)
{
    error.clear();
    if (manifest.syntaxRevision.empty() || manifest.semanticRevision.empty() ||
        manifest.win64AbiRevision.empty() || manifest.metadataRevision.empty())
    { error = "freeze manifest requires syntax, semantic, Win64 ABI, and metadata revisions"; return false; }
    return true;
}

bool validateSyntaxFreeze(const SyntaxFreezeManifest& manifest, std::string& error)
{
    error.clear();
    if (manifest.grammarRevision.empty() || manifest.canonicalGrammarHash.empty() ||
        manifest.precedenceHash.empty() || manifest.reservedWordsHash.empty() ||
        manifest.recoveryRulesHash.empty())
    { error = "syntax freeze requires versioned hashes for every grammar surface"; return false; }
    if (!manifest.generatedFromCanonicalSource)
    { error = "reference grammar must be generated from its canonical source"; return false; }
    if (!manifest.parserCorpusPassed || !manifest.ambiguityCheckPassed ||
        !manifest.formatterRoundTripPassed || !manifest.specificationSnippetsPassed)
    { error = "syntax freeze evidence is incomplete"; return false; }
    return true;
}

bool validateSemanticFreeze(const SemanticFreezeManifest& manifest, std::string& error)
{
    error.clear();
    if (manifest.overloadRevision.empty() || manifest.conversionRevision.empty() ||
        manifest.inferenceRevision.empty() || manifest.ownershipAndDropRevision.empty() ||
        manifest.errorRevision.empty() || manifest.asyncRevision.empty() ||
        manifest.memoryModelRevision.empty())
    { error = "semantic freeze requires a revision for every normative subsystem"; return false; }
    for (const std::string& milestone : manifest.deferredIssueMilestones)
    {
        if (!milestone.starts_with("3.1") && !milestone.starts_with("3.2") &&
            !milestone.starts_with("4."))
        { error = "semantic freeze exceptions must target a post-3.0 milestone"; return false; }
    }
    if (!manifest.normativeSuitePassed || !manifest.differentialConfigurationsDeterministic)
    { error = "semantic freeze evidence is incomplete"; return false; }
    return true;
}

bool validateWin64AbiFreeze(const Win64AbiFreezeManifest& manifest, std::string& error)
{
    error.clear();
    if (manifest.target != "windows-x64")
    { error = "NativeSharp 3.0 ABI freeze is restricted to windows-x64"; return false; }
    if (manifest.manglingRevision.empty() || manifest.layoutRevision.empty() ||
        manifest.callingConventionRevision.empty() || manifest.variantRevision.empty() ||
        manifest.traitObjectRevision.empty() || manifest.errorRevision.empty() ||
        manifest.publicCoroutineRevision.empty() || manifest.moduleMetadataRevision.empty())
    { error = "Win64 ABI freeze requires a revision for every public ABI surface"; return false; }
    if (!manifest.preservedDllFixturesPassed || !manifest.crossVersionConsumerPassed ||
        !manifest.symbolSnapshotsPassed || !manifest.pdbContractPassed)
    { error = "Win64 ABI freeze evidence is incomplete"; return false; }
    return true;
}

bool validateConformanceManifest(
    std::span<const ConformanceCase> cases,
    std::string& error)
{
    error.clear();
    std::unordered_set<std::string> identities;
    bool mandatory = false;
    for (const auto& item : cases)
    {
        if (item.identity.empty() || !identities.insert(item.identity).second)
        { error = "conformance cases require unique non-empty identities"; return false; }
        mandatory = mandatory || item.tier == ConformanceTier::Mandatory;
    }
    if (!mandatory) { error = "conformance manifest has no mandatory cases"; return false; }
    return true;
}

bool validateCandidateConformance(
    const CandidateConformanceManifest& manifest,
    std::string& error)
{
    error.clear();
    if (manifest.suiteVersion.empty() || manifest.resultSchemaVersion.empty() ||
        !validateConformanceManifest(manifest.cases, error))
    {
        if (error.empty()) error = "candidate conformance suite requires versioned suite and result schemas";
        return false;
    }
    bool recommended = false;
    bool implementationSpecific = false;
    for (const ConformanceCase& item : manifest.cases)
    {
        recommended = recommended || item.tier == ConformanceTier::Recommended;
        implementationSpecific = implementationSpecific || item.tier == ConformanceTier::ImplementationSpecific;
    }
    if (!recommended || !implementationSpecific)
    { error = "candidate conformance suite must separate all three tiers"; return false; }
    if (!manifest.selfValidationPassed || !manifest.brokenCompilerRejected ||
        !manifest.shardRetryEquivalent || !manifest.hermeticRunPassed)
    { error = "candidate conformance execution evidence is incomplete"; return false; }
    return true;
}

bool bilingualSpecificationInParity(
    std::span<const BilingualClause> clauses,
    std::string& error)
{
    error.clear();
    std::unordered_set<std::string> identities;
    for (const auto& clause : clauses)
    {
        if (clause.identity.empty() || clause.spanishRequirement.empty() ||
            clause.englishRequirement.empty() || clause.spanishExamples != clause.englishExamples ||
            !identities.insert(clause.identity).second)
        { error = "bilingual clauses require shared unique IDs, both requirements, and example parity"; return false; }
    }
    return !clauses.empty();
}

bool validateBilingualSpecificationAudit(
    std::span<const BilingualClause> clauses,
    std::string& error)
{
    if (!bilingualSpecificationInParity(clauses, error)) return false;
    for (const BilingualClause& clause : clauses)
    {
        if (clause.spanishCompatibilityNotes != clause.englishCompatibilityNotes)
        { error = "bilingual clause compatibility notes are not in parity"; return false; }
        if (!clause.glossaryConsistent)
        { error = "bilingual clause violates the shared glossary"; return false; }
        if (!clause.linksValid || !clause.codeSamplesCompile || !clause.presentInFinalIndexes)
        { error = "bilingual clause has broken links, samples, or index ownership"; return false; }
    }
    return true;
}

bool changeAllowed(ReleaseCandidateStage stage, ChangeSeverity severity) noexcept
{
    if (stage == ReleaseCandidateStage::RC1) return severity != ChangeSeverity::Feature;
    if (stage == ReleaseCandidateStage::RC2) return severity == ChangeSeverity::Fix ||
        severity == ChangeSeverity::PromotionBlocker;
    return severity == ChangeSeverity::PromotionBlocker;
}

bool validateReleaseCandidate(
    const ReleaseCandidateEvidence& evidence,
    std::string& error)
{
    error.clear();
    if (evidence.identity.empty() || evidence.featureSetHash.empty() || evidence.knownIssuesHash.empty())
    { error = "release candidate requires identity, feature-set hash, and known-issues hash"; return false; }
    for (const ChangeSeverity severity : evidence.acceptedChanges)
    {
        if (!changeAllowed(evidence.stage, severity))
        { error = "release candidate contains a change forbidden at its stage"; return false; }
    }
    if (!evidence.acceptedChanges.empty() && !evidence.affectedGatesRestarted)
    { error = "release candidate changes require affected gates to restart"; return false; }
    if (!evidence.cleanMatrixPassed || !evidence.metadataUpgradeDowngradePassed ||
        !evidence.reproducible || !evidence.longRunningStressPassed ||
        !evidence.packageFreeSamplesPassed)
    { error = "release candidate evidence is incomplete"; return false; }
    return true;
}

PromotionStatus promotionStatus(const PromotionEvidence& evidence) noexcept
{
    return evidence.externalBaselinePassed && evidence.priorityZeroIssues == 0 &&
        evidence.unwaivedPriorityOneIssues == 0 && evidence.debugConformancePassed &&
        evidence.releaseConformancePassed && evidence.bilingualDocsInParity &&
        evidence.migrationValidated && evidence.artifactsReproducible && evidence.hashesPublished
        ? PromotionStatus::Ready : PromotionStatus::NotReady;
}

std::vector<std::string> missingPromotionEvidence(const PromotionEvidence& evidence)
{
    std::vector<std::string> result;
    if (!evidence.externalBaselinePassed) result.emplace_back("external-baseline");
    if (evidence.priorityZeroIssues != 0) result.emplace_back("p0-issues");
    if (evidence.unwaivedPriorityOneIssues != 0) result.emplace_back("unwaived-p1-issues");
    if (!evidence.debugConformancePassed) result.emplace_back("debug-conformance");
    if (!evidence.releaseConformancePassed) result.emplace_back("release-conformance");
    if (!evidence.bilingualDocsInParity) result.emplace_back("bilingual-docs");
    if (!evidence.migrationValidated) result.emplace_back("migration");
    if (!evidence.artifactsReproducible) result.emplace_back("reproducible-artifacts");
    if (!evidence.hashesPublished) result.emplace_back("published-hashes");
    return result;
}

std::string_view stableLanguageVersion() noexcept { return "3.0.0"; }
std::string_view normativeTarget() noexcept { return "windows-x64"; }
}
