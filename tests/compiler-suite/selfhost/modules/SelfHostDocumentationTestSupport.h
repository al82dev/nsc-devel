#pragma once

#include "nsc_suite/TestRegistry.h"

#include <string>
#include <string_view>

namespace nsc::suite::selfhost_documentation_support
{
struct PendingCase final
{
    std::string_view caseIdentifier;
    std::string_view contractIdentifier;
    std::string_view oracle;
    std::string_view requirement;
};

inline void addInfrastructurePendingCase(TestRegistry& registry, const PendingCase& pending)
{
    const std::string requirement(pending.requirement);
    registry.add(TestCase{
        std::string(pending.caseIdentifier),
        std::string(pending.contractIdentifier),
        "SelfHost.Documentation",
        "build-contract",
        "NIGHTLY",
        {"selfhost", "documentation", "infrastructure-pending", std::string(pending.oracle)},
        [requirement](TestContext& context)
        {
            context.expect(false, requirement);
        },
        TestDisposition::InfrastructurePending});
}
}
