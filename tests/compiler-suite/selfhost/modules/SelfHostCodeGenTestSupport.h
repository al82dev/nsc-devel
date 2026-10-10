#pragma once

#include "nsc_suite/TestRegistry.h"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace nsc::suite::selfhost_codegen_support
{
// Compactly describes one canonical contract while retaining enough information to materialize
// every individual Phase 1 ID, layer, profile and oracle without copying product algorithms.
struct PendingContract final
{
    std::size_t ordinal;
    std::size_t firstCaseOrdinal;
    std::size_t caseCount;
    std::size_t nightlyCount;
    std::string_view layer;
    std::string_view oracle;
};

inline std::string caseId(std::size_t ordinal)
{
    std::ostringstream stream;
    stream << "TSH-" << std::setw(5) << std::setfill('0') << ordinal;
    return stream.str();
}

inline std::string contractId(std::size_t ordinal)
{
    std::ostringstream stream;
    stream << "CSH-CG-" << std::setw(3) << std::setfill('0') << ordinal;
    return stream.str();
}

// Registers one case per canonical ID under the non-executable infrastructure disposition. The
// body remains defensively red if an unsupported direct invocation bypasses normal selection.
inline void addInfrastructureContract(
    TestRegistry& registry,
    const PendingContract& contract,
    std::string_view requirement)
{
    for (std::size_t offset = 0U; offset < contract.caseCount; ++offset)
    {
        const bool nightly = offset >= contract.caseCount - contract.nightlyCount;
        const std::string reason(requirement);
        registry.add(TestCase{
            caseId(contract.firstCaseOrdinal + offset),
            contractId(contract.ordinal),
            "SelfHost.CodeGen",
            std::string(contract.layer),
            nightly ? "NIGHTLY" : "PR",
            {"selfhost", "codegen", "infrastructure-pending", std::string(contract.oracle)},
            [reason](TestContext& context)
            {
                context.expect(false, reason);
            },
            TestDisposition::InfrastructurePending});
    }
}
}
