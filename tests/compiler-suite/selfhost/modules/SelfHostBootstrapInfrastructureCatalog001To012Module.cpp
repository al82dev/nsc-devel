#include "SelfHostBootstrapInfrastructureTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_bootstrap_infrastructure_support::PendingContractRange;

constexpr std::string_view kInfrastructureRequirement =
    "SelfHost.Bootstrap.Infrastructure requires nsc0, nsc1, nsc2, NSLink, LLVM, Python, and the self-host contract producers before the typed bootstrap oracle can be executed";

constexpr std::array<PendingContractRange, 12> kContracts{{
    {1081, 1086, "C12B-BSI-001", "BOOTSTRAP_DATA",
     "selfhost/bootstrap/bootstrap-profile.json",
     "parseo tipado y correspondencia exacta con productores/consumidores", kInfrastructureRequirement},
    {1087, 1092, "C12B-BSI-002", "BOOTSTRAP_DATA",
     "selfhost/bootstrap/bootstrap.config.example.json",
     "parseo tipado y correspondencia exacta con productores/consumidores", kInfrastructureRequirement},
    {1093, 1098, "C12B-BSI-003", "BOOTSTRAP_DATA",
     "selfhost/bootstrap/bootstrap.lock",
     "parseo tipado y correspondencia exacta con productores/consumidores", kInfrastructureRequirement},
    {1099, 1104, "C12B-BSI-004", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/clean_selfhost.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1105, 1110, "C12B-BSI-005", "BOOTSTRAP_DATA",
     "selfhost/bootstrap/dependencies.md",
     "parseo tipado y correspondencia exacta con productores/consumidores", kInfrastructureRequirement},
    {1111, 1116, "C12B-BSI-006", "BOOTSTRAP_DATA",
     "selfhost/bootstrap/gate.template.json",
     "parseo tipado y correspondencia exacta con productores/consumidores", kInfrastructureRequirement},
    {1117, 1122, "C12B-BSI-007", "BOOTSTRAP_DATA",
     "selfhost/bootstrap/generations.json",
     "parseo tipado y correspondencia exacta con productores/consumidores", kInfrastructureRequirement},
    {1123, 1128, "C12B-BSI-008", "BOOTSTRAP_DATA",
     "selfhost/bootstrap/path-configuration.md",
     "parseo tipado y correspondencia exacta con productores/consumidores", kInfrastructureRequirement},
    {1129, 1134, "C12B-BSI-009", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/profile_selection.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1135, 1140, "C12B-BSI-010", "BOOTSTRAP_DATA",
     "selfhost/bootstrap/README.md",
     "parseo tipado y correspondencia exacta con productores/consumidores", kInfrastructureRequirement},
    {1141, 1146, "C12B-BSI-011", "BOOTSTRAP_DATA",
     "selfhost/bootstrap/recovery.md",
     "parseo tipado y correspondencia exacta con productores/consumidores", kInfrastructureRequirement},
    {1147, 1152, "C12B-BSI-012", "BOOTSTRAP_DATA",
     "selfhost/bootstrap/report.template.md",
     "parseo tipado y correspondencia exacta con productores/consumidores", kInfrastructureRequirement},
}};
}

void registerSelfHostBootstrapInfrastructureCatalog001To012Module(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_bootstrap_infrastructure_support::addInfrastructurePendingRange(registry, range);
    }
}
}

