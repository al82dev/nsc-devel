#include "SelfHostSemaTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_sema_support::PendingContractRange;

constexpr std::string_view kModuleSetRequirement =
    "the controller has no executable NativeSharp selfhost module-set boundary with isolated source graphs, "
    "external NSMETA fixtures, two-pass publication, and exact dump observation";

constexpr std::array<PendingContractRange, 5> kContracts{{
    {737, 745, "CSH-SEM-006", "component", "index rows lookups overload counts and problems",
     kModuleSetRequirement},
    {746, 753, "CSH-SEM-007", "component", "state transitions counts model and errors",
     kModuleSetRequirement},
    {754, 760, "CSH-SEM-008", "integration", "external counts parsed records exports and diagnostic",
     kModuleSetRequirement},
    {761, 769, "CSH-SEM-009", "integration", "unit states index exports resolutions and diagnostic",
     kModuleSetRequirement},
    {770, 777, "CSH-SEM-010", "unit", "exact normalized report", kModuleSetRequirement},
}};
}

void registerSelfHostSemaModuleSetModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_sema_support::addInfrastructurePendingRange(registry, range);
    }
}
}
