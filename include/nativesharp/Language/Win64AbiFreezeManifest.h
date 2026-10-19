#pragma once

#include <string>

namespace nativesharp::language
{
struct Win64AbiFreezeManifest final
{
    std::string target;
    std::string manglingRevision;
    std::string layoutRevision;
    std::string callingConventionRevision;
    std::string variantRevision;
    std::string traitObjectRevision;
    std::string errorRevision;
    std::string publicCoroutineRevision;
    std::string moduleMetadataRevision;
    bool preservedDllFixturesPassed = false;
    bool crossVersionConsumerPassed = false;
    bool symbolSnapshotsPassed = false;
    bool pdbContractPassed = false;
};
}
