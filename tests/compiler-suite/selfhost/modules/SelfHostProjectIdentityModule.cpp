#include "SelfHostProjectTestSupport.h"

namespace nsc::suite
{
namespace
{
constexpr selfhost_project_support::PendingContractRange kIdentityContract{
    579,
    586,
    "CSH-PRO-001",
    "resolved source set compared with canonical 27 sources",
    "the controller has no isolated selfhost project admission executor with mutable project XML, "
    "source-glob resolution, dependency producers, and a canonical 27-source oracle"};
}

void registerSelfHostProjectIdentityModule(TestRegistry& registry)
{
    selfhost_project_support::addInfrastructurePendingRange(registry, kIdentityContract);
}
}
