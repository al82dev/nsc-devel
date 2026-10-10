#include "SelfHostProjectsTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_projects_support::PendingContractRange;

constexpr std::string_view kIdentityRequirement =
    "the controller has no selfhost project dump/NSMETA/cache executor with controlled metadata "
    "producers, stable-byte capture, mutation injection, and canonical hash vectors";

constexpr std::array<PendingContractRange, 3> kContracts{{
    {623, 630, "CSH-PRJ-004", "unit", "exact normalized dump and source order",
     kIdentityRequirement},
    {631, 638, "CSH-PRJ-005", "integration",
     "header producer ABI compatibility fields and decision", kIdentityRequirement},
    {639, 645, "CSH-PRJ-006", "unit", "known vectors and mutation sensitivity",
     kIdentityRequirement},
}};
}

void registerSelfHostProjectsIdentityModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_projects_support::addInfrastructurePendingRange(registry, range);
    }
}
}
