#pragma once

#include <string>
#include <vector>

#include "nativesharp/Language/ChangeSeverity.h"
#include "nativesharp/Language/ReleaseCandidateStage.h"

namespace nativesharp::language
{
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
}
