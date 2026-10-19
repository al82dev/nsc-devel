#pragma once

#include "nativesharp/Interop/LibraryMetadataGenericParameterContract.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::interop
{
struct LibraryMetadataGenericTemplate final
{
    using ParameterContract = LibraryMetadataGenericParameterContract;

    std::uint8_t kind = 0;
    std::uint8_t accessibility = 0;
    std::string canonicalKey;
    std::string moduleName;
    std::string namespaceName;
    std::string ownerName;
    std::string name;
    std::vector<std::string> parameters;
    // NSMETA 1.7: declaration slice as authored by the producer. Public entries are reusable API;
    // non-public entries are materialization-only dependencies and retain their original access.
    // The consumer parses both in the producer's synthetic project, while only public roots are
    // nameable from the consumer project.
    std::string sourceText;
    // NSMETA 1.11: canonical template identities directly required by this declaration/body.
    // Same-module edges must be present in the file; external-module edges are resolved from the
    // consumer's other LibraryReferences.
    std::vector<std::string> dependencies;
    std::vector<ParameterContract> parameterContracts;
    std::vector<std::string> lifetimeParameters;
    std::vector<std::pair<std::string, std::string>> lifetimeOutlivesConstraints;
};
}
