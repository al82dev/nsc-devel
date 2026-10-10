#include "SelfHostProjectsTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_projects_support::PendingContractRange;

constexpr std::string_view kArtifactRequirement =
    "the controller has no selfhost toolchain/product/source-graph executor with controlled "
    "producer identities, output-root fixtures, graph ownership, and deterministic diagnostics";

constexpr std::array<PendingContractRange, 3> kContracts{{
    {646, 653, "CSH-PRJ-007", "integration", "parsed fields and exact stable ordering",
     kArtifactRequirement},
    {654, 661, "CSH-PRJ-008", "component", "artifact list extensions roots and order",
     kArtifactRequirement},
    {662, 669, "CSH-PRJ-009", "unit", "unit/project tables order and diagnostic",
     kArtifactRequirement},
}};
}

void registerSelfHostProjectsArtifactsModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_projects_support::addInfrastructurePendingRange(registry, range);
    }
}
}
