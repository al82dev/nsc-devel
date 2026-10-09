#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/ReleaseSemantics.h"

#include <array>
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
    registry.add(TestCase{std::move(id), "CL-LANG-018", "Host.Language", "unit", "PR",
        {"language", "release"}, std::move(function)});
}

ReleaseCandidateEvidence completeReleaseCandidate()
{
    return {ReleaseCandidateStage::RC2, "native-sharp-3.0-rc2", "features-sha", "issues-sha",
        {ChangeSeverity::Fix}, true, true, true, true, true, true};
}

PromotionEvidence completePromotionEvidence()
{
    return {true, 0, 0, true, true, true, true, true, true};
}

OfficialPromotionManifest completeOfficialManifest()
{
    return {completePromotionEvidence(), "features-sha", "specification-sha", "suite-sha", "v.3.0",
        true, true, true, true, true};
}

void registerChangePolicyCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-239", [](TestContext& context) {
        struct Sample final
        {
            ReleaseCandidateStage stage;
            std::array<bool, 3> allowed;
        };
        constexpr std::array samples = {
            Sample{ReleaseCandidateStage::RC1, {false, true, true}},
            Sample{ReleaseCandidateStage::RC2, {false, true, true}},
            Sample{ReleaseCandidateStage::RC3, {false, false, true}},
        };
        constexpr std::array severities = {ChangeSeverity::Feature, ChangeSeverity::Fix,
            ChangeSeverity::PromotionBlocker};
        for (const Sample& sample : samples)
            for (std::size_t index = 0; index < severities.size(); ++index)
                context.expect(changeAllowed(sample.stage, severities[index]) == sample.allowed[index],
                    "TL-LANG-239 the complete three-stage by three-severity policy must be exact");
    });
}

void registerValidCandidateCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-240", [](TestContext& context) {
        std::string error = "stale";
        auto unchanged = completeReleaseCandidate();
        unchanged.acceptedChanges.clear();
        unchanged.affectedGatesRestarted = false;
        context.expect(validateReleaseCandidate(unchanged, error) && error.empty(),
            "TL-LANG-240 a complete candidate without changes needs no gate restart");
        const auto changed = completeReleaseCandidate();
        context.expect(validateReleaseCandidate(changed, error) && error.empty() &&
                changed.affectedGatesRestarted && changed.cleanMatrixPassed &&
                changed.metadataUpgradeDowngradePassed && changed.reproducible &&
                changed.longRunningStressPassed && changed.packageFreeSamplesPassed,
            "TL-LANG-240 an allowed change with restarted complete gates must pass unchanged");
    });
}

void registerCandidateRestartCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-241", [](TestContext& context) {
        auto candidate = completeReleaseCandidate();
        candidate.affectedGatesRestarted = false;
        std::string error;
        context.expect(!validateReleaseCandidate(candidate, error) &&
                error == "release candidate changes require affected gates to restart",
            "TL-LANG-241 an accepted change without restarted gates must report the exact error");
    });
}

void registerPromotionMatrixCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-242", [](TestContext& context) {
        constexpr unsigned combinations = 1U << 9U;
        for (unsigned mask = 0; mask < combinations; ++mask)
        {
            PromotionEvidence evidence;
            evidence.externalBaselinePassed = (mask & (1U << 0U)) != 0;
            evidence.priorityZeroIssues = (mask & (1U << 1U)) != 0 ? 0U : 1U;
            evidence.unwaivedPriorityOneIssues = (mask & (1U << 2U)) != 0 ? 0U : 1U;
            evidence.debugConformancePassed = (mask & (1U << 3U)) != 0;
            evidence.releaseConformancePassed = (mask & (1U << 4U)) != 0;
            evidence.bilingualDocsInParity = (mask & (1U << 5U)) != 0;
            evidence.migrationValidated = (mask & (1U << 6U)) != 0;
            evidence.artifactsReproducible = (mask & (1U << 7U)) != 0;
            evidence.hashesPublished = (mask & (1U << 8U)) != 0;
            const PromotionStatus expected = mask == combinations - 1U ?
                PromotionStatus::Ready : PromotionStatus::NotReady;
            context.expect(promotionStatus(evidence) == expected,
                "TL-LANG-242 all 512 promotion-evidence combinations must classify exactly");
        }
    });
}

void registerMissingEvidenceCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-243", [](TestContext& context) {
        struct Sample final { PromotionEvidence evidence; const char* missing; };
        auto baseline = completePromotionEvidence(); baseline.externalBaselinePassed = false;
        auto p0 = completePromotionEvidence(); p0.priorityZeroIssues = 1;
        auto p1 = completePromotionEvidence(); p1.unwaivedPriorityOneIssues = 1;
        auto debug = completePromotionEvidence(); debug.debugConformancePassed = false;
        auto release = completePromotionEvidence(); release.releaseConformancePassed = false;
        auto docs = completePromotionEvidence(); docs.bilingualDocsInParity = false;
        auto migration = completePromotionEvidence(); migration.migrationValidated = false;
        auto artifacts = completePromotionEvidence(); artifacts.artifactsReproducible = false;
        auto hashes = completePromotionEvidence(); hashes.hashesPublished = false;
        const std::array samples = {
            Sample{baseline, "external-baseline"}, Sample{p0, "p0-issues"},
            Sample{p1, "unwaived-p1-issues"}, Sample{debug, "debug-conformance"},
            Sample{release, "release-conformance"}, Sample{docs, "bilingual-docs"},
            Sample{migration, "migration"}, Sample{artifacts, "reproducible-artifacts"},
            Sample{hashes, "published-hashes"},
        };
        for (const Sample& sample : samples)
            context.expect(missingPromotionEvidence(sample.evidence) ==
                    std::vector<std::string>{sample.missing},
                "TL-LANG-243 each single missing evidence item must produce its exact singleton");
        PromotionEvidence allMissing;
        allMissing.priorityZeroIssues = 1;
        allMissing.unwaivedPriorityOneIssues = 1;
        const std::vector<std::string> expected = {"external-baseline", "p0-issues",
            "unwaived-p1-issues", "debug-conformance", "release-conformance", "bilingual-docs",
            "migration", "reproducible-artifacts", "published-hashes"};
        context.expect(missingPromotionEvidence(allMissing) == expected,
            "TL-LANG-243 all missing evidence must be reported once in stable contractual order");
    });
}

void registerOfficialReadyCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-244", [](TestContext& context) {
        std::vector<std::string> missing{"stale"};
        context.expect(officialPromotionStatus(completeOfficialManifest(), missing) ==
                    PromotionStatus::Ready && missing.empty(),
            "TL-LANG-244 a complete official manifest must be promotable and clear stale missing items");
    });
}

void registerOfficialMissingCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-245", [](TestContext& context) {
        struct Sample final { OfficialPromotionManifest manifest; const char* missing; };
        auto feature = completeOfficialManifest(); feature.stableFeatureSetHash.clear();
        auto specification = completeOfficialManifest(); specification.specificationHash.clear();
        auto suite = completeOfficialManifest(); suite.conformanceSuiteHash.clear();
        auto branch = completeOfficialManifest(); branch.maintenanceBranch = "development";
        auto boundary = completeOfficialManifest(); boundary.experimentalFeaturesNamedAndFlagged = false;
        auto install = completeOfficialManifest(); install.referenceWin64InstallPassed = false;
        auto smoke = completeOfficialManifest(); smoke.smokeInteropPassed = false;
        auto samplesMissing = completeOfficialManifest(); samplesMissing.samplesPassed = false;
        auto artifacts = completeOfficialManifest(); artifacts.artifactsVerified = false;
        const std::array samples = {
            Sample{feature, "feature-set-hash"}, Sample{specification, "specification-hash"},
            Sample{suite, "conformance-suite-hash"}, Sample{branch, "maintenance-branch"},
            Sample{boundary, "experimental-feature-boundary"},
            Sample{install, "reference-win64-install"}, Sample{smoke, "smoke-interop"},
            Sample{samplesMissing, "samples"}, Sample{artifacts, "artifact-verification"},
        };
        std::vector<std::string> missing;
        for (const Sample& sample : samples)
            context.expect(officialPromotionStatus(sample.manifest, missing) ==
                        PromotionStatus::NotReady && missing == std::vector<std::string>{sample.missing},
                "TL-LANG-245 each absent official item must produce its exact singleton");
        auto allMissing = completeOfficialManifest();
        allMissing.stableFeatureSetHash.clear();
        allMissing.specificationHash.clear();
        allMissing.conformanceSuiteHash.clear();
        allMissing.maintenanceBranch.clear();
        allMissing.experimentalFeaturesNamedAndFlagged = false;
        allMissing.referenceWin64InstallPassed = false;
        allMissing.smokeInteropPassed = false;
        allMissing.samplesPassed = false;
        allMissing.artifactsVerified = false;
        const std::vector<std::string> expected = {"feature-set-hash", "specification-hash",
            "conformance-suite-hash", "maintenance-branch", "experimental-feature-boundary",
            "reference-win64-install", "smoke-interop", "samples", "artifact-verification"};
        context.expect(officialPromotionStatus(allMissing, missing) == PromotionStatus::NotReady &&
                missing == expected,
            "TL-LANG-245 all absent official items must be reported in stable contractual order");
    });
}

void registerStableIdentityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-246", [](TestContext& context) {
        context.expect(stableLanguageVersion() == "3.0.0" && normativeTarget() == "windows-x64",
            "TL-LANG-246 stable language version and normative target must remain exact");
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-247", [](TestContext& context) {
        std::string error;
        std::vector<std::string> missing;
        for (unsigned repetition = 0; repetition < 64; ++repetition)
        {
            const auto validCandidate = completeReleaseCandidate();
            auto invalidCandidate = validCandidate;
            invalidCandidate.identity.clear();
            context.expect(validateReleaseCandidate(validCandidate, error) && error.empty() &&
                    !validateReleaseCandidate(invalidCandidate, error) &&
                    error == "release candidate requires identity, feature-set hash, and known-issues hash" &&
                    validateReleaseCandidate(validCandidate, error) && error.empty(),
                "TL-LANG-247 alternating candidate validation must be deterministic and stateless");
            auto invalidOfficial = completeOfficialManifest();
            invalidOfficial.maintenanceBranch = "development";
            context.expect(officialPromotionStatus(completeOfficialManifest(), missing) ==
                        PromotionStatus::Ready && missing.empty() &&
                    officialPromotionStatus(invalidOfficial, missing) == PromotionStatus::NotReady &&
                    missing == std::vector<std::string>{"maintenance-branch"} &&
                    officialPromotionStatus(completeOfficialManifest(), missing) ==
                        PromotionStatus::Ready && missing.empty(),
                "TL-LANG-247 alternating promotion calls must reset output and retain no global state");
        }
    });
}

void registerHistoricalPolicyCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-248", [](TestContext& context) {
        std::vector<std::string> missing;
        auto historical = completeOfficialManifest();
        auto later = historical;
        later.maintenanceBranch = "v.3.0.4";
        context.expect(stableLanguageVersion() == "3.0.0" && historical.maintenanceBranch == "v.3.0" &&
                officialPromotionStatus(historical, missing) == PromotionStatus::Ready && missing.empty() &&
                officialPromotionStatus(later, missing) == PromotionStatus::NotReady &&
                missing == std::vector<std::string>{"maintenance-branch"},
            "TL-LANG-248 historical version and branch values remain the exact current contract");
    });
}
}

void registerLanguageReleasePromotionModule(TestRegistry& registry)
{
    registerChangePolicyCase(registry);
    registerValidCandidateCase(registry);
    registerCandidateRestartCase(registry);
    registerPromotionMatrixCase(registry);
    registerMissingEvidenceCase(registry);
    registerOfficialReadyCase(registry);
    registerOfficialMissingCase(registry);
    registerStableIdentityCase(registry);
    registerDeterminismCase(registry);
    registerHistoricalPolicyCase(registry);
}
}
