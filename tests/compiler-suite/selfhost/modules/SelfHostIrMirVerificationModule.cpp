#include "SelfHostIrTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_ir_support::PendingContractRange;

constexpr std::string_view kMirRequirement =
    "the controller has no selfhost MIR/CFG executor with real frontend provenance, cleanup "
    "lifetime tracking, failure injection, and an independent graph reconstruction oracle";

constexpr std::array<PendingContractRange, 4> kContracts{{
    {338, 346, "CSH-IR-005", "component", "operation sequence and result description",
     kMirRequirement},
    {347, 353, "CSH-IR-006", "component", "independent CFG reconstruction and block operations",
     kMirRequirement},
    {354, 361, "CSH-IR-007", "component", "cleanup identities order and verifier problem",
     kMirRequirement},
    {362, 368, "CSH-IR-008", "unit", "independent graph invariant result", kMirRequirement},
}};
}

void registerSelfHostIrMirVerificationModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_ir_support::addInfrastructurePendingRange(registry, range);
    }
}
}
