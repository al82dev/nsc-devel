#include "nativesharp/Language/ReleaseSemantics.h"

#include "../support/TestContext.h"

#include <array>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;
    tests.expect(validateFreezeManifest({"syntax-v1", "semantics-v1", "win64-abi-v1", "nsmeta-v3"}, error) &&
            !validateFreezeManifest({"syntax-v1", "", "win64-abi-v1", "nsmeta-v3"}, error),
        "freeze manifest requires independent syntax, semantic, ABI, and metadata revisions");

    const std::array<ConformanceCase, 3> conformance = {{{"parser.tuples", ConformanceTier::Mandatory},
        {"quality.diagnostics", ConformanceTier::Recommended},
        {"backend.llvm", ConformanceTier::ImplementationSpecific}}};
    tests.expect(validateConformanceManifest(conformance, error),
        "conformance manifest separates mandatory, recommended, and implementation tiers");
    const std::array<BilingualClause, 2> clauses = {{{"NS3-OWN-001", "requisito", "requirement", 2, 2},
        {"NS3-ABI-001", "requisito", "requirement", 1, 1}}};
    tests.expect(bilingualSpecificationInParity(clauses, error),
        "bilingual specification clauses share IDs and example counts");

    tests.expect(!changeAllowed(ReleaseCandidateStage::RC1, ChangeSeverity::Feature) &&
            changeAllowed(ReleaseCandidateStage::RC1, ChangeSeverity::Fix) &&
            !changeAllowed(ReleaseCandidateStage::RC3, ChangeSeverity::Fix) &&
            changeAllowed(ReleaseCandidateStage::RC3, ChangeSeverity::PromotionBlocker),
        "RC1 freezes features, RC2 accepts fixes, and RC3 accepts promotion blockers only");

    const PromotionEvidence missing{};
    tests.expect(promotionStatus(missing) == PromotionStatus::NotReady &&
            missingPromotionEvidence(missing).size() == 7,
        "promotion fails closed while external baseline and release evidence are absent");
    const PromotionEvidence complete{true, 0, 0, true, true, true, true, true, true};
    tests.expect(promotionStatus(complete) == PromotionStatus::Ready &&
            missingPromotionEvidence(complete).empty(),
        "promotion becomes ready only when every final gate has evidence");
    tests.expect(stableLanguageVersion() == "3.0.0" && normativeTarget() == "windows-x64",
        "release contract fixes NativeSharp language 3.0.0 to Windows x64");
    return tests.result();
}
