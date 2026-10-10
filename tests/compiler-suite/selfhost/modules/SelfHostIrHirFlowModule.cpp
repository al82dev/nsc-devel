#include "SelfHostIrTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_ir_support::PendingContractRange;

constexpr std::string_view kIrRequirement =
    "the controller has no selfhost IR model executor with owned semantic inputs, independent "
    "record/graph oracles, malformed-model construction, and isolated lifetime observation";

constexpr std::array<PendingContractRange, 4> kContracts{{
    {307, 314, "CSH-IR-001", "unit", "exact records order fields and count", kIrRequirement},
    {315, 323, "CSH-IR-002", "component", "independent walk of node/record provenance",
     kIrRequirement},
    {324, 330, "CSH-IR-003", "unit", "independent invariant checker result", kIrRequirement},
    {331, 337, "CSH-IR-004", "unit", "node/edge/local tables and mutations", kIrRequirement},
}};
}

void registerSelfHostIrHirFlowModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_ir_support::addInfrastructurePendingRange(registry, range);
    }
}
}
