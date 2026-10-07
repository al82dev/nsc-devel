#include "nativesharp/Language/ReleaseSemantics.h"

#include <algorithm>
#include <iostream>

using namespace nativesharp::language;

int main()
{
    int failures = 0;
    int checks = 0;
    const auto check = [&](bool value, const char* message) {
        ++checks; if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }};
    OfficialPromotionManifest manifest;
    manifest.evidence = {true, 0, 0, true, true, true, true, true, true};
    manifest.stableFeatureSetHash = "features-sha";
    manifest.specificationHash = "spec-sha";
    manifest.conformanceSuiteHash = "suite-sha";
    manifest.maintenanceBranch = "v.3.0";
    manifest.experimentalFeaturesNamedAndFlagged = true;
    manifest.referenceWin64InstallPassed = true;
    manifest.smokeInteropPassed = true;
    manifest.samplesPassed = true;
    manifest.artifactsVerified = true;
    std::vector<std::string> missing;
    check(officialPromotionStatus(manifest, missing) == PromotionStatus::Ready && missing.empty(),
        "complete official promotion evidence is ready");
    auto changed = manifest; changed.evidence.externalBaselinePassed = false;
    check(officialPromotionStatus(changed, missing) == PromotionStatus::NotReady &&
        std::ranges::find(missing, "external-baseline") != missing.end(), "missing baseline fails closed");
    changed = manifest; changed.conformanceSuiteHash.clear();
    check(officialPromotionStatus(changed, missing) == PromotionStatus::NotReady,
        "missing conformance-suite hash fails");
    changed = manifest; changed.stableFeatureSetHash.clear();
    check(officialPromotionStatus(changed, missing) == PromotionStatus::NotReady, "missing feature-set hash fails");
    changed = manifest; changed.specificationHash.clear();
    check(officialPromotionStatus(changed, missing) == PromotionStatus::NotReady, "missing specification hash fails");
    changed = manifest; changed.maintenanceBranch = "development";
    check(officialPromotionStatus(changed, missing) == PromotionStatus::NotReady, "missing maintenance branch fails");
    changed = manifest; changed.experimentalFeaturesNamedAndFlagged = false;
    check(officialPromotionStatus(changed, missing) == PromotionStatus::NotReady, "unbounded experimental features fail");
    changed = manifest; changed.referenceWin64InstallPassed = false;
    check(officialPromotionStatus(changed, missing) == PromotionStatus::NotReady, "reference install failure blocks promotion");
    changed = manifest; changed.samplesPassed = false;
    check(officialPromotionStatus(changed, missing) == PromotionStatus::NotReady, "sample failure blocks promotion");
    changed = manifest; changed.artifactsVerified = false;
    check(officialPromotionStatus(changed, missing) == PromotionStatus::NotReady, "unverified artifacts block promotion");
    if (failures != 0) return 1;
    std::cout << "PASS: " << checks << " official promotion checks\n";
    return 0;
}
