#include "SelfHostContractsTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_contracts_support::PendingContractRange;

constexpr std::string_view kInfrastructureRequirement =
    "SelfHost.Contracts requires the typed bootstrap contract loader, the self-host fixtures, and SH41 producer artifacts before its observable oracle can be executed";

constexpr std::array<PendingContractRange, 12> kContracts{{
    {1591, 1596, "C12B-SHC-013",
     "selfhost/contracts/frontend.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1597, 1602, "C12B-SHC-014",
     "selfhost/contracts/functional-equivalence.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1603, 1608, "C12B-SHC-015",
     "selfhost/contracts/generics-aot.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1609, 1614, "C12B-SHC-016",
     "selfhost/contracts/image-replay-retirement.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1615, 1620, "C12B-SHC-017",
     "selfhost/contracts/interop-base.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1621, 1626, "C12B-SHC-018",
     "selfhost/contracts/interop-parity.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1627, 1632, "C12B-SHC-019",
     "selfhost/contracts/ir.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1633, 1638, "C12B-SHC-020",
     "selfhost/contracts/llvm-backend.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1639, 1644, "C12B-SHC-021",
     "selfhost/contracts/llvm-c-api.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1645, 1650, "C12B-SHC-022",
     "selfhost/contracts/low-level-parity.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1651, 1656, "C12B-SHC-023",
     "selfhost/contracts/module-set.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1657, 1662, "C12B-SHC-024",
     "selfhost/contracts/native-text.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
}};
}

void registerSelfHostContractsIrRuntimeModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_contracts_support::addInfrastructurePendingRange(registry, range);
    }
}
}

