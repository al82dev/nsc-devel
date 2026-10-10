#include "SelfHostIrTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_ir_support::PendingContractRange;

constexpr std::string_view kLifecycleRequirement =
    "the controller has no selfhost IR lifecycle executor with independent dominance, normalized "
    "dump, pass-pipeline, admission-state, and first-failure oracles";

constexpr std::array<PendingContractRange, 4> kContracts{{
    {369, 376, "CSH-IR-009", "component", "independent dominator oracle and iteration stability",
     kLifecycleRequirement},
    {377, 383, "CSH-IR-010", "unit", "exact normalized structural text", kLifecycleRequirement},
    {384, 391, "CSH-IR-011", "unit", "exact pass list by level", kLifecycleRequirement},
    {392, 398, "CSH-IR-012", "integration", "valid flag model identities dumps and first failure",
     kLifecycleRequirement},
}};
}

void registerSelfHostIrDominanceLifecycleModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_ir_support::addInfrastructurePendingRange(registry, range);
    }
}
}
