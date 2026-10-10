#include "SelfHostSyntaxTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_syntax_support::PendingContractRange;

constexpr std::string_view kNodesRequirement =
    "the controller has no executable NativeSharp selfhost syntax-node boundary exposing scalar fields, "
    "setters, slots, ordered children, duplicates, and relationship invariants";

constexpr std::array<PendingContractRange, 2> kContracts{{
    {945, 950, "CSH-SYN-001", "all field values before/after setter", kNodesRequirement},
    {951, 958, "CSH-SYN-002", "slots children order and count", kNodesRequirement},
}};
}

void registerSelfHostSyntaxNodesModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_syntax_support::addInfrastructurePendingRange(registry, range);
    }
}
}
