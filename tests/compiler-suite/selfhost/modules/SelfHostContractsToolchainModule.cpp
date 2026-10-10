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
    {1519, 1524, "C12B-SHC-001",
     "selfhost/contracts/advanced-toolchain.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1525, 1530, "C12B-SHC-002",
     "selfhost/contracts/arrays-memory.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1531, 1536, "C12B-SHC-003",
     "selfhost/contracts/backend-general.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1537, 1542, "C12B-SHC-004",
     "selfhost/contracts/closure.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1543, 1548, "C12B-SHC-005",
     "selfhost/contracts/cross-machine-determinism.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1549, 1554, "C12B-SHC-006",
     "selfhost/contracts/data-layout.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1555, 1560, "C12B-SHC-007",
     "selfhost/contracts/developer-experience.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1561, 1566, "C12B-SHC-008",
     "selfhost/contracts/diagnostics.map.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1567, 1572, "C12B-SHC-009",
     "selfhost/contracts/distribution-final.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1573, 1578, "C12B-SHC-010",
     "selfhost/contracts/final-certification.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1579, 1584, "C12B-SHC-011",
     "selfhost/contracts/first-generation.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1585, 1590, "C12B-SHC-012",
     "selfhost/contracts/fixed-point.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
}};
}

void registerSelfHostContractsToolchainModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_contracts_support::addInfrastructurePendingRange(registry, range);
    }
}
}

