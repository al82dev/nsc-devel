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
    {1153, 1158, "C12B-BSI-013", "BOOTSTRAP_DATA",
     "selfhost/bootstrap/run_sh44_machine.cmd",
     "parseo tipado y correspondencia exacta con productores/consumidores", kInfrastructureRequirement},
    {1159, 1164, "C12B-BSI-014", "BOOTSTRAP_DATA",
     "selfhost/bootstrap/schemas/bootstrap-lock.schema.json",
     "parseo tipado y correspondencia exacta con productores/consumidores", kInfrastructureRequirement},
    {1165, 1170, "C12B-BSI-015", "BOOTSTRAP_DATA",
     "selfhost/bootstrap/schemas/bootstrap-profile.schema.json",
     "parseo tipado y correspondencia exacta con productores/consumidores", kInfrastructureRequirement},
    {1171, 1176, "C12B-BSI-016", "BOOTSTRAP_DATA",
     "selfhost/bootstrap/schemas/gate.schema.json",
     "parseo tipado y correspondencia exacta con productores/consumidores", kInfrastructureRequirement},
    {1177, 1182, "C12B-BSI-017", "BOOTSTRAP_DATA",
     "selfhost/bootstrap/schemas/report.schema.json",
     "parseo tipado y correspondencia exacta con productores/consumidores", kInfrastructureRequirement},
    {1183, 1188, "C12B-BSI-018", "BOOTSTRAP_DATA",
     "selfhost/bootstrap/SH44_PEER_README.txt",
     "parseo tipado y correspondencia exacta con productores/consumidores", kInfrastructureRequirement},
    {1189, 1194, "C12B-BSI-019", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_advanced_toolchain.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1195, 1200, "C12B-BSI-020", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_arrays_memory.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1201, 1206, "C12B-BSI-021", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_backend_general.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1207, 1212, "C12B-BSI-022", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_bootstrap_dependencies.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1213, 1218, "C12B-BSI-023", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_bootstrap_surface.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1219, 1224, "C12B-BSI-024", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_callable_graph.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
}};
}

void registerSelfHostBootstrapInfrastructureCatalog013To024Module(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_bootstrap_infrastructure_support::addInfrastructurePendingRange(registry, range);
    }
}
}

