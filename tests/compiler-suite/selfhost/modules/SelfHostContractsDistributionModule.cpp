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
    {1663, 1668, "C12B-SHC-025",
     "selfhost/contracts/object-model.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1669, 1674, "C12B-SHC-026",
     "selfhost/contracts/performance.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1675, 1680, "C12B-SHC-027",
     "selfhost/contracts/program-ir.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1681, 1686, "C12B-SHC-028",
     "selfhost/contracts/project-driver.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1687, 1692, "C12B-SHC-029",
     "selfhost/contracts/promotion-readiness.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1693, 1698, "C12B-SHC-030",
     "selfhost/contracts/README.md",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1699, 1704, "C12B-SHC-031",
     "selfhost/contracts/repeated-fixed-point.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1705, 1710, "C12B-SHC-032",
     "selfhost/contracts/robustness-final.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1711, 1716, "C12B-SHC-033",
     "selfhost/contracts/robustness.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1717, 1722, "C12B-SHC-034",
     "selfhost/contracts/runtime-bootstrap.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1723, 1728, "C12B-SHC-035",
     "selfhost/contracts/runtime-library.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1729, 1734, "C12B-SHC-036",
     "selfhost/contracts/self-hosted-generation.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
}};
}

void registerSelfHostContractsDistributionModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_contracts_support::addInfrastructurePendingRange(registry, range);
    }
}
}

