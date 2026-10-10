#include "SelfHostProjectTestSupport.h"

namespace nsc::suite
{
namespace
{
constexpr selfhost_project_support::PendingContractRange kInventoryContract{
    594,
    600,
    "CSH-PRO-003",
    "resolved paths cardinality uniqueness and component ownership",
    "the controller has no isolated project/filesystem inventory executor with controlled globs, "
    "duplicate injection, ownership classification, and scale-boundary observation"};
}

void registerSelfHostProjectInventoryModule(TestRegistry& registry)
{
    selfhost_project_support::addInfrastructurePendingRange(registry, kInventoryContract);
}
}
