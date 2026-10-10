#pragma once

#include "nsc_suite/TestRegistry.h"

#include <array>
#include <cstdio>
#include <string>
#include <string_view>

namespace nsc::suite::selfhost_project_support
{
struct PendingContractRange final
{
    int firstOrdinal;
    int lastOrdinal;
    std::string_view contractIdentifier;
    std::string_view oracle;
    std::string_view requirement;
};

inline std::string caseIdentifier(int ordinal)
{
    std::array<char, 10> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "TSH-%05d", ordinal);
    return std::string(buffer.data());
}

inline void addInfrastructurePendingRange(
    TestRegistry& registry, const PendingContractRange& range)
{
    const std::string requirement(range.requirement);
    for (int ordinal = range.firstOrdinal; ordinal <= range.lastOrdinal; ++ordinal)
    {
        registry.add(TestCase{
            caseIdentifier(ordinal),
            std::string(range.contractIdentifier),
            "SelfHost.Project",
            "build-contract",
            "NIGHTLY",
            {"selfhost", "project", "infrastructure-pending", std::string(range.oracle)},
            [requirement](TestContext& context)
            {
                context.expect(false, requirement);
            },
            TestDisposition::InfrastructurePending});
    }
}
}
