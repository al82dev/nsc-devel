#pragma once

#include "nsc_suite/TestRegistry.h"

#include <array>
#include <cstdio>
#include <string>
#include <string_view>

namespace nsc::suite::selfhost_bootstrap_infrastructure_support
{
struct PendingContractRange final
{
    int firstOrdinal;
    int lastOrdinal;
    std::string_view contractIdentifier;
    std::string_view category;
    std::string_view sourcePath;
    std::string_view oracle;
    std::string_view requirement;
};

inline std::string caseIdentifier(int ordinal)
{
    std::array<char, 11> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "T12B-%05d", ordinal);
    return std::string(buffer.data());
}

inline std::string_view inputClass(int ordinal, int firstOrdinal)
{
    constexpr std::array<std::string_view, 6> classes{
        "valid-minimal",
        "valid-representative",
        "invalid-precondition",
        "invalid-structure",
        "boundary",
        "determinism",
    };
    return classes[static_cast<std::size_t>(ordinal - firstOrdinal)];
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
            "SelfHost.Bootstrap.Infrastructure",
            "self-host",
            "NIGHTLY",
            {
                "selfhost",
                "bootstrap-infrastructure",
                "infrastructure-pending",
                std::string(inputClass(ordinal, range.firstOrdinal)),
                std::string(range.category),
                std::string(range.sourcePath),
                std::string(range.oracle),
            },
            [requirement](TestContext& context)
            {
                context.expect(false, requirement);
            },
            TestDisposition::InfrastructurePending});
    }
}
}

