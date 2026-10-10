#pragma once

#include "nsc_suite/TestRegistry.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite::ast_infrastructure_support
{
enum class AstPendingDisposition
{
    InfrastructurePending,
};

struct InfrastructurePendingCase final
{
    std::string caseIdentifier;
    std::string contractIdentifier;
    std::string layer;
    std::string profile;
    std::string trigger;
    std::string oracle;
    std::string requirement;
    AstPendingDisposition disposition = AstPendingDisposition::InfrastructurePending;
};

inline std::vector<InfrastructurePendingCase>& infrastructurePendingCases()
{
    static std::vector<InfrastructurePendingCase> cases;
    return cases;
}

inline void addInfrastructurePendingCase(TestRegistry& registry, std::string caseIdentifier,
    std::string contractIdentifier, std::string layer, std::string trigger,
    std::string oracle, std::string requirement)
{
    // Infrastructure-special cases remain materialized outside the ordinary
    // executable registry. They are neither PASS, SKIP, nor product defects.
    (void)registry;
    auto& cases = infrastructurePendingCases();
    const auto found = std::find_if(cases.begin(), cases.end(),
        [&caseIdentifier](const InfrastructurePendingCase& candidate)
        {
            return candidate.caseIdentifier == caseIdentifier;
        });
    if (found == cases.end())
    {
        cases.push_back(InfrastructurePendingCase{std::move(caseIdentifier),
            std::move(contractIdentifier), std::move(layer), "NIGHTLY", std::move(trigger),
            std::move(oracle), std::move(requirement)});
    }
}
}
