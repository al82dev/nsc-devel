#include "SelfHostProjectsTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_projects_support::PendingContractRange;

constexpr std::string_view kModelRequirement =
    "the controller has no selfhost ProjectModel/XML executor with mutable filesystem fixtures, "
    "owned parsing diagnostics, source provenance, and exact model observation";

constexpr std::array<PendingContractRange, 3> kContracts{{
    {601, 607, "CSH-PRJ-001", "unit", "exact fields counts and indexed values", kModelRequirement},
    {608, 614, "CSH-PRJ-002", "component", "project model and first failure", kModelRequirement},
    {615, 622, "CSH-PRJ-003", "component", "validation status reason and selected path",
     kModelRequirement},
}};
}

void registerSelfHostProjectsModelModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_projects_support::addInfrastructurePendingRange(registry, range);
    }
}
}
