#pragma once

#include "nsc_suite/TestRegistry.h"

#include <array>
#include <cstdio>
#include <string>
#include <string_view>

namespace nsc::suite::selfhost_sema_support
{
struct PendingContractRange final
{
    int firstOrdinal;
    int lastOrdinal;
    std::string_view contractIdentifier;
    std::string_view layer;
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
    constexpr std::array<int, 23> nightlies{
        709, 710, 718, 719, 727, 728, 736, 744, 745, 752, 753, 760,
        768, 769, 777, 791, 799, 806, 819, 826, 833, 840, 848};
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
            "SelfHost.Sema",
            std::string(range.layer),
            isNightly(ordinal) ? "NIGHTLY" : "PR",
            {"selfhost", "sema", "infrastructure-pending", std::string(range.oracle)},
            [requirement](TestContext& context)
            {
                context.expect(false, requirement);
            },
            TestDisposition::InfrastructurePending});
    }
}
}
