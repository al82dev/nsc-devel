#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/ReleaseSemantics.h"

#include <array>
#include <string>
#include <utility>

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

CandidateConformanceManifest completeCandidateManifest()
{
    return {"suite-v1", "results-v1",
        {{"mandatory", ConformanceTier::Mandatory}, {"recommended", ConformanceTier::Recommended},
            {"implementation", ConformanceTier::ImplementationSpecific}},
        true, true, true, true};
}

BilingualClause completeClause(std::string identity = "NS-001")
{
    return {std::move(identity), "requisito", "requirement", 2, 2, 1, 1, true, true, true, true};
}

void registerConformanceManifestCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-235", [](TestContext& context) {
        std::string error;
        const std::array<ConformanceCase, 1> mandatory = {{{"mandatory", ConformanceTier::Mandatory}}};
        context.expect(validateConformanceManifest(mandatory, error) && error.empty(),
            "TL-LANG-235 one unique mandatory case is a valid minimum manifest");
        const std::array<ConformanceCase, 1> recommended = {
            {{"recommended", ConformanceTier::Recommended}}};
        context.expect(!validateConformanceManifest(recommended, error) &&
                error == "conformance manifest has no mandatory cases" &&
                !validateConformanceManifest(std::array<ConformanceCase, 0>{}, error) &&
                error == "conformance manifest has no mandatory cases",
            "TL-LANG-235 manifests without Mandatory cases must report the exact error");
        const std::array<ConformanceCase, 2> emptyIdentity = {{{"", ConformanceTier::Mandatory},
            {"valid", ConformanceTier::Recommended}}};
        const std::array<ConformanceCase, 2> duplicate = {{{"same", ConformanceTier::Mandatory},
            {"same", ConformanceTier::ImplementationSpecific}}};
        context.expect(!validateConformanceManifest(emptyIdentity, error) &&
                error == "conformance cases require unique non-empty identities" &&
                !validateConformanceManifest(duplicate, error) &&
                error == "conformance cases require unique non-empty identities",
            "TL-LANG-235 empty and duplicate identities must report the exact identity error");
    });
}

void registerCandidateConformanceCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-236", [](TestContext& context) {
        std::string error;
        context.expect(validateCandidateConformance(completeCandidateManifest(), error) && error.empty(),
            "TL-LANG-236 a versioned three-tier candidate with four evidence gates must pass");
        for (std::size_t version = 0; version < 2; ++version)
        {
            auto manifest = completeCandidateManifest();
            if (version == 0) manifest.suiteVersion.clear();
            else manifest.resultSchemaVersion.clear();
            context.expect(!validateCandidateConformance(manifest, error) &&
                    error == "candidate conformance suite requires versioned suite and result schemas",
                "TL-LANG-236 each absent candidate schema version must report the exact error");
        }
        for (const ConformanceTier missing : {ConformanceTier::Recommended,
                ConformanceTier::ImplementationSpecific})
        {
            auto manifest = completeCandidateManifest();
            manifest.cases.erase(manifest.cases.begin() + (missing == ConformanceTier::Recommended ? 1 : 2));
            context.expect(!validateCandidateConformance(manifest, error) &&
                    error == "candidate conformance suite must separate all three tiers",
                "TL-LANG-236 missing Recommended or ImplementationSpecific tier must fail exactly");
        }
        for (std::size_t gate = 0; gate < 4; ++gate)
        {
            auto manifest = completeCandidateManifest();
            std::array<bool*, 4> gates = {&manifest.selfValidationPassed,
                &manifest.brokenCompilerRejected, &manifest.shardRetryEquivalent,
                &manifest.hermeticRunPassed};
            *gates[gate] = false;
            context.expect(!validateCandidateConformance(manifest, error) &&
                    error == "candidate conformance execution evidence is incomplete",
                "TL-LANG-236 each absent execution gate must report the exact error");
        }
    });
}

void registerBilingualParityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-237", [](TestContext& context) {
        std::string error = "stale";
        const std::array valid = {completeClause("NS-001"), completeClause("NS-002")};
        context.expect(bilingualSpecificationInParity(valid, error) && error.empty(),
            "TL-LANG-237 unique bilingual clauses with matching examples must pass");
        context.expect(!bilingualSpecificationInParity(std::array<BilingualClause, 0>{}, error) && error.empty(),
            "TL-LANG-237 an empty clause set must fail without manufacturing a diagnostic");
        auto emptyIdentity = completeClause(); emptyIdentity.identity.clear();
        auto spanishMissing = completeClause(); spanishMissing.spanishRequirement.clear();
        auto englishMissing = completeClause(); englishMissing.englishRequirement.clear();
        auto exampleMismatch = completeClause(); ++exampleMismatch.englishExamples;
        const std::array invalid = {emptyIdentity, spanishMissing, englishMissing, exampleMismatch};
        for (const BilingualClause& clause : invalid)
            context.expect(!bilingualSpecificationInParity(std::array{clause}, error) &&
                    error == "bilingual clauses require shared unique IDs, both requirements, and example parity",
                "TL-LANG-237 every malformed bilingual clause must report the exact shared error");
        const std::array duplicate = {completeClause("same"), completeClause("same")};
        context.expect(!bilingualSpecificationInParity(duplicate, error) &&
                error == "bilingual clauses require shared unique IDs, both requirements, and example parity",
            "TL-LANG-237 duplicate bilingual identities must report the exact shared error");
    });
}

void registerBilingualAuditCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-238", [](TestContext& context) {
        std::string error;
        context.expect(validateBilingualSpecificationAudit(std::array{completeClause()}, error) && error.empty(),
            "TL-LANG-238 a complete bilingual audit record must pass");
        auto notes = completeClause(); ++notes.englishCompatibilityNotes;
        context.expect(!validateBilingualSpecificationAudit(std::array{notes}, error) &&
                error == "bilingual clause compatibility notes are not in parity",
            "TL-LANG-238 compatibility-note mismatch must be the first exact error");
        auto glossary = completeClause(); glossary.glossaryConsistent = false;
        context.expect(!validateBilingualSpecificationAudit(std::array{glossary}, error) &&
                error == "bilingual clause violates the shared glossary",
            "TL-LANG-238 glossary mismatch must report the exact error");
        for (std::size_t gate = 0; gate < 3; ++gate)
        {
            auto clause = completeClause();
            std::array<bool*, 3> gates = {
                &clause.linksValid, &clause.codeSamplesCompile, &clause.presentInFinalIndexes};
            *gates[gate] = false;
            context.expect(!validateBilingualSpecificationAudit(std::array{clause}, error) &&
                    error == "bilingual clause has broken links, samples, or index ownership",
                "TL-LANG-238 each absent link, sample, or index gate must report the exact error");
        }
    });
}
}

void registerLanguageReleaseConformanceModule(TestRegistry& registry)
{
    registerConformanceManifestCase(registry);
    registerCandidateConformanceCase(registry);
    registerBilingualParityCase(registry);
    registerBilingualAuditCase(registry);
}
}
