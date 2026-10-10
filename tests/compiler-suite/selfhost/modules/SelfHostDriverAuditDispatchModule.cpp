#include "SelfHostDriverTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_driver_support::PendingContractRange;

constexpr std::string_view kAuditRequirement =
    "the controller has no selfhost Driver executor for project audit reports, frozen dependency "
    "identities, response-file expansion, recursive-boundary observation, and isolated processes";

constexpr std::array<PendingContractRange, 3> kContracts{{
    {284, 292, "CSH-DRV-008", "report records ordering identities counts and exit code",
     kAuditRequirement},
    {293, 299, "CSH-DRV-009", "artifact identities reasons and final status", kAuditRequirement},
    {300, 306, "CSH-DRV-010", "selected handler arguments recursion boundary and exit code",
     kAuditRequirement},
}};
}

void registerSelfHostDriverAuditDispatchModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_driver_support::addInfrastructurePendingRange(registry, range);
    }
}
}
