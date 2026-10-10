#pragma once

#include "nsc_suite/TestRegistry.h"

#include <array>
#include <cstdio>
#include <string>
#include <string_view>

namespace nsc::suite::selfhost_parser_support
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
    constexpr std::array<int, 20> nightlies{
        472, 479, 486, 494, 495, 502, 503, 511, 512, 519,
        527, 535, 536, 544, 545, 553, 561, 569, 577, 578};
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
            "SelfHost.Parser",
            std::string(range.layer),
            isNightly(ordinal) ? "NIGHTLY" : "PR",
            {"selfhost", "parser", "infrastructure-pending", std::string(range.oracle)},
            [requirement](TestContext& context)
            {
                context.expect(false, requirement);
            },
            TestDisposition::InfrastructurePending});
    }
}
}
