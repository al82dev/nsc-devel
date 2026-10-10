#include "SelfHostDriverTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_driver_support::PendingContractRange;

constexpr std::string_view kEmissionRequirement =
    "the controller has no selfhost emission executor that owns compiler stages, toolchain, "
    "temporary artifacts, diagnostics, and partial-publication failure injection";

constexpr std::array<PendingContractRange, 3> kContracts{{
    {260, 267, "CSH-DRV-005", "stage order artifact bytes and exit code", kEmissionRequirement},
    {268, 275, "CSH-DRV-006", "unit order stage diagnostics artifacts and exit code",
     kEmissionRequirement},
    {276, 283, "CSH-DRV-007", "verified stage sequence artifact and exit result",
     kEmissionRequirement},
}};
}

void registerSelfHostDriverEmissionModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_driver_support::addInfrastructurePendingRange(registry, range);
    }
}
}
