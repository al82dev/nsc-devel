#pragma once

#include "nsc_suite/TestRegistry.h"

#include <string>
#include <string_view>

namespace nsc::suite::selfhost_bootstrap_support
{
// One Phase 1 case whose product oracle cannot yet be reached by the controller.
// These records are intentionally kept outside the ordinary executable manifest.
struct PendingCase final
{
    std::string_view caseIdentifier;
    std::string_view contractIdentifier;
    std::string_view layer;
    std::string_view profile;
    std::string_view oracle;
    std::string_view requirement;
};

// Registers metadata with the host disposition dedicated to missing infrastructure. The
// defensive body prevents an accidental direct invocation from manufacturing a PASS.
inline void addInfrastructurePendingCase(TestRegistry& registry, const PendingCase& pending)
{
    const std::string requirement(pending.requirement);
    registry.add(TestCase{
        std::string(pending.caseIdentifier),
        std::string(pending.contractIdentifier),
        "SelfHost.Bootstrap",
        std::string(pending.layer),
        std::string(pending.profile),
        {"selfhost", "bootstrap", "infrastructure-pending", std::string(pending.oracle)},
        [requirement](TestContext& context)
        {
            context.expect(false, requirement);
        },
        TestDisposition::InfrastructurePending});
}
}
