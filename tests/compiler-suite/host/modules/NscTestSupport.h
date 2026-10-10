#pragma once

#include "nsc_suite/TestRegistry.h"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace nsc::suite::nsc_support
{
// Identifies one canonical Host.nsc contract and the infrastructure that must exist before its
// cases can become executable. The strings are static literals owned by each consolidated module.
struct PendingContract
{
    std::size_t ordinal;
    std::string_view reason;
};

// Builds the stable case identifier assigned by the Phase 1 design. Eight variants belong to
// every contract, so the mapping is O(1), allocation-bounded, and independent of registration order.
inline std::string caseId(std::size_t contractOrdinal, std::size_t variant)
{
    const std::size_t ordinal = (contractOrdinal - 1U) * 8U + variant;
    std::ostringstream stream;
    stream << "TF-NSC-" << std::setw(3) << std::setfill('0') << ordinal;
    return stream.str();
}

// Builds the stable contract identifier assigned by the Phase 1 inventory. Formatting is kept in
// one place so every module emits the same zero-padded identity without duplicating string logic.
inline std::string contractId(std::size_t ordinal)
{
    std::ostringstream stream;
    stream << "CF-NSC-" << std::setw(3) << std::setfill('0') << ordinal;
    return stream.str();
}

// Registers the eight designed variants of a blocked contract without making them selectable by
// ordinary execution. The failing body is defensive: even an incorrect direct invocation cannot
// manufacture PASS while the required private seam or process executor remains absent.
inline void addInfrastructureContract(
    TestRegistry& registry,
    const PendingContract& contract,
    std::string layer)
{
    const std::string reason(contract.reason);
    for (std::size_t variant = 1U; variant <= 8U; ++variant)
    {
        registry.add(TestCase{
            caseId(contract.ordinal, variant),
            contractId(contract.ordinal),
            "Host.nsc",
            layer,
            variant == 8U ? "NIGHTLY" : "PR",
            {"nsc", "infrastructure-pending", "variant-" + std::to_string(variant)},
            [reason](TestContext& context)
            {
                context.expect(false, reason);
            },
            TestDisposition::InfrastructurePending});
    }
}
}
