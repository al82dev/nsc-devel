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
    struct SourceImport final
    {
        std::string namespaceName;
        // Empty for a namespace import; generic aliases retain their parameter list.
        std::string alias;
        std::string target;
        std::uint8_t importKind = 0;
        bool reexport = false;
        bool operator==(const SourceImport&) const = default;
    };

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
    // NSMETA 1.41: exact lexical directives, in source scope order.
    std::vector<SourceImport> sourceImports;
    // NSMETA 1.42: candidate nominal receiver types needed for source reconstruction.
    std::vector<std::string> candidateReceiverTypeKeys;
};
}
