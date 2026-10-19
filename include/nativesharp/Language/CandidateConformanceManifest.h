#pragma once

#include <string>
#include <vector>

#include "nativesharp/Language/ConformanceCase.h"

namespace nativesharp::language
{
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
}
