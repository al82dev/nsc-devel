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
    {1735, 1740, "C12B-SHC-037",
     "selfhost/contracts/semantic.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1741, 1746, "C12B-SHC-038",
     "selfhost/contracts/source-bool-void.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1747, 1752, "C12B-SHC-039",
     "selfhost/contracts/source-control-flow.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1753, 1758, "C12B-SHC-040",
     "selfhost/contracts/source-derived-backend.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1759, 1764, "C12B-SHC-041",
     "selfhost/contracts/source-expressions.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1765, 1770, "C12B-SHC-042",
     "selfhost/contracts/source-floating-scalars.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1771, 1776, "C12B-SHC-043",
     "selfhost/contracts/source-functions.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1777, 1782, "C12B-SHC-044",
     "selfhost/contracts/source-graph.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1783, 1788, "C12B-SHC-045",
     "selfhost/contracts/source-integer-widths.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1789, 1794, "C12B-SHC-046",
     "selfhost/contracts/source-loop-control.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1795, 1800, "C12B-SHC-047",
     "selfhost/contracts/source-pointers.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
    {1801, 1806, "C12B-SHC-048",
     "selfhost/contracts/specialized-code.contract.json",
     "campos tipados, cardinalidades, paths y expectativas declaradas", kInfrastructureRequirement},
}};
}

void registerSelfHostContractsSourceModule(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_contracts_support::addInfrastructurePendingRange(registry, range);
    }
}
}

