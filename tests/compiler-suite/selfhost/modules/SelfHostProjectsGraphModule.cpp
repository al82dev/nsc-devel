#include "SelfHostProjectsTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_projects_support::PendingContractRange;

constexpr std::string_view kGraphRequirement =
    "the controller has no selfhost include/source-graph executor with sandboxed directory "
    "enumeration, dependency/cycle injection, deterministic ordering, and exact graph dump capture";

constexpr std::array<PendingContractRange, 3> kContracts{{
    {670, 678, "CSH-PRJ-010", "component", "normalized paths sort order duplicates and error",
     kGraphRequirement},
    {679, 687, "CSH-PRJ-011", "integration", "project/unit order edges and diagnostic",
     kGraphRequirement},
    {688, 695, "CSH-PRJ-012", "unit", "exact normalized report", kGraphRequirement},
}};
}

void registerSelfHostProjectsGraphModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_projects_support::addInfrastructurePendingRange(registry, range);
    }
}
}
