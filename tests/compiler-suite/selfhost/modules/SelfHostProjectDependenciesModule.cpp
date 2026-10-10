#include "SelfHostProjectTestSupport.h"

namespace nsc::suite
{
namespace
{
constexpr selfhost_project_support::PendingContractRange kDependencyContract{
    587,
    593,
    "CSH-PRO-002",
    "exact normalized reference set and order",
    "the controller has no isolated selfhost project dependency executor with mutable references, "
    "four real dependency producers, and normalized order/error capture"};
}

void registerSelfHostProjectDependenciesModule(TestRegistry& registry)
{
    selfhost_project_support::addInfrastructurePendingRange(registry, kDependencyContract);
}
}
