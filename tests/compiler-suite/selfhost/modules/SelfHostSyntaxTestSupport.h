#pragma once

#include "nsc_suite/TestRegistry.h"

#include <array>
#include <cstdio>
#include <string>
#include <string_view>

namespace nsc::suite::selfhost_syntax_support
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

inline bool isNightly(int ordinal)
{
    constexpr std::array<int, 3> nightlies{958, 965, 973};
    for (const int candidate : nightlies)
    {
        if (candidate == ordinal)
        {
            return true;
        }
    }
    return false;
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
            "SelfHost.Syntax",
            "unit",
            isNightly(ordinal) ? "NIGHTLY" : "PR",
            {"selfhost", "syntax", "infrastructure-pending", std::string(range.oracle)},
            [requirement](TestContext& context)
            {
                context.expect(false, requirement);
            },
            TestDisposition::InfrastructurePending});
    }
}
}
