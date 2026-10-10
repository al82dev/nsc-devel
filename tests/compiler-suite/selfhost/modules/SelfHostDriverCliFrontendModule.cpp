#include "SelfHostDriverTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_driver_support::PendingContractRange;

constexpr std::string_view kProcessRequirement =
    "the controller has no selfhost Driver process executor with owned argv, streams, source "
    "fixtures, environment, output capture, and failure-boundary observation";

constexpr std::array<PendingContractRange, 4> kContracts{{
    {228, 235, "CSH-DRV-001", "exit code streams and semantic fields", kProcessRequirement},
    {236, 244, "CSH-DRV-002", "normalized dump text and exit code", kProcessRequirement},
    {245, 252, "CSH-DRV-003", "selected dump exact structure and exit code", kProcessRequirement},
    {253, 259, "CSH-DRV-004", "check identities and final exit code", kProcessRequirement},
}};
}

void registerSelfHostDriverCliFrontendModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_driver_support::addInfrastructurePendingRange(registry, range);
    }
}
}
