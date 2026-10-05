#include "nativesharp/Language/ReleaseSemantics.h"

#include <iostream>

using namespace nativesharp::language;

int main()
{
    int failures = 0;
    int checks = 0;
    const auto check = [&](bool value, const char* message) {
        ++checks; if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }};
    CandidateConformanceManifest manifest{"ns3-conformance-rc1", "result-v1",
        {{"syntax.tuple", ConformanceTier::Mandatory},
         {"quality.diagnostic", ConformanceTier::Recommended},
         {"backend.llvm", ConformanceTier::ImplementationSpecific}}, true, true, true, true};
    std::string error;
    check(validateCandidateConformance(manifest, error), "complete candidate conformance manifest passes");
    auto changed = manifest; changed.suiteVersion.clear();
    check(!validateCandidateConformance(changed, error), "unversioned suite fails");
    changed = manifest; changed.resultSchemaVersion.clear();
    check(!validateCandidateConformance(changed, error), "unversioned result schema fails");
    changed = manifest; changed.cases.erase(changed.cases.begin());
    check(!validateCandidateConformance(changed, error), "missing mandatory tier fails");
    changed = manifest; changed.cases.erase(changed.cases.begin() + 1);
    check(!validateCandidateConformance(changed, error), "missing recommended tier fails");
    changed = manifest; changed.cases.pop_back();
    check(!validateCandidateConformance(changed, error), "missing implementation tier fails");
    changed = manifest; changed.selfValidationPassed = false;
    check(!validateCandidateConformance(changed, error), "failed self-validation blocks candidate");
    changed = manifest; changed.brokenCompilerRejected = false;
    check(!validateCandidateConformance(changed, error), "undetected broken compiler blocks candidate");
    changed = manifest; changed.shardRetryEquivalent = false;
    check(!validateCandidateConformance(changed, error), "shard/retry drift blocks candidate");
    changed = manifest; changed.hermeticRunPassed = false;
    check(!validateCandidateConformance(changed, error), "non-hermetic run blocks candidate");
    if (failures != 0) return 1;
    std::cout << "PASS: " << checks << " conformance candidate checks\n";
    return 0;
}
