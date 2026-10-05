#include "nativesharp/Language/ReleaseSemantics.h"

#include <iostream>

using namespace nativesharp::language;

int main()
{
    int failures = 0;
    int checks = 0;
    const auto check = [&](bool value, const char* message) {
        ++checks; if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }};
    ReleaseCandidateEvidence rc;
    rc.stage = ReleaseCandidateStage::RC2;
    rc.identity = "native-sharp-3.0-rc2";
    rc.featureSetHash = "feature-sha";
    rc.knownIssuesHash = "issues-sha";
    rc.acceptedChanges = {ChangeSeverity::Fix};
    rc.affectedGatesRestarted = true;
    rc.cleanMatrixPassed = true;
    rc.metadataUpgradeDowngradePassed = true;
    rc.reproducible = true;
    rc.longRunningStressPassed = true;
    rc.packageFreeSamplesPassed = true;
    std::string error;
    check(validateReleaseCandidate(rc, error), "complete RC2 evidence passes");
    auto changed = rc; changed.identity.clear();
    check(!validateReleaseCandidate(changed, error), "missing RC identity fails");
    changed = rc; changed.acceptedChanges = {ChangeSeverity::Feature};
    check(!validateReleaseCandidate(changed, error), "feature change after RC1 freeze fails");
    changed = rc; changed.affectedGatesRestarted = false;
    check(!validateReleaseCandidate(changed, error), "changed RC without gate restart fails");
    changed = rc; changed.cleanMatrixPassed = false;
    check(!validateReleaseCandidate(changed, error), "missing clean matrix fails");
    changed = rc; changed.metadataUpgradeDowngradePassed = false;
    check(!validateReleaseCandidate(changed, error), "metadata transition failure blocks RC");
    changed = rc; changed.reproducible = false;
    check(!validateReleaseCandidate(changed, error), "non-reproducible RC fails");
    changed = rc; changed.longRunningStressPassed = false;
    check(!validateReleaseCandidate(changed, error), "stress failure blocks RC");
    changed = rc; changed.packageFreeSamplesPassed = false;
    check(!validateReleaseCandidate(changed, error), "package-free sample failure blocks RC");
    changed = rc; changed.stage = ReleaseCandidateStage::RC3;
    changed.acceptedChanges = {ChangeSeverity::Fix};
    check(!validateReleaseCandidate(changed, error), "ordinary fix is forbidden in RC3");
    if (failures != 0) return 1;
    std::cout << "PASS: " << checks << " release candidate checks\n";
    return 0;
}
