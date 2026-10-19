#pragma once

#include <string>
#include <vector>

namespace nativesharp::language
{
struct SemanticFreezeManifest final
{
    std::string overloadRevision;
    std::string conversionRevision;
    std::string inferenceRevision;
    std::string ownershipAndDropRevision;
    std::string errorRevision;
    std::string asyncRevision;
    std::string memoryModelRevision;
    std::vector<std::string> deferredIssueMilestones;
    bool normativeSuitePassed = false;
    bool differentialConfigurationsDeterministic = false;
};
}
