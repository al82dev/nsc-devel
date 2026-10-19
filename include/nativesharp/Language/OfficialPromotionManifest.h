#pragma once

#include <string>

#include "nativesharp/Language/PromotionEvidence.h"

namespace nativesharp::language
{
struct OfficialPromotionManifest final
{
    PromotionEvidence evidence;
    std::string stableFeatureSetHash;
    std::string specificationHash;
    std::string conformanceSuiteHash;
    std::string maintenanceBranch;
    bool experimentalFeaturesNamedAndFlagged = false;
    bool referenceWin64InstallPassed = false;
    bool smokeInteropPassed = false;
    bool samplesPassed = false;
    bool artifactsVerified = false;
};
}
