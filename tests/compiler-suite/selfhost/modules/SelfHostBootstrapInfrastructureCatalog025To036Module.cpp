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
    {1225, 1230, "C12B-BSI-025", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_closure.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1231, 1236, "C12B-BSI-026", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_cross_machine_determinism.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1237, 1242, "C12B-BSI-027", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_data_layout.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1243, 1248, "C12B-BSI-028", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_developer_experience.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1249, 1254, "C12B-BSI-029", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_distribution_final.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1255, 1260, "C12B-BSI-030", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_driver_products.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1261, 1266, "C12B-BSI-031", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_eh_regions.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1267, 1272, "C12B-BSI-032", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_environment.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1273, 1278, "C12B-BSI-033", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_final_certification.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1279, 1284, "C12B-BSI-034", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_first_generation.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1285, 1290, "C12B-BSI-035", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_fixed_point.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1291, 1296, "C12B-BSI-036", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_frontend.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
}};
}

void registerSelfHostBootstrapInfrastructureCatalog025To036Module(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_bootstrap_infrastructure_support::addInfrastructurePendingRange(registry, range);
    }
}
}

