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
    {1369, 1374, "C12B-BSI-049", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_object_model.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1375, 1380, "C12B-BSI-050", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_ownership_cleanup.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1381, 1386, "C12B-BSI-051", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_performance.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1387, 1392, "C12B-BSI-052", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_profile.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1393, 1398, "C12B-BSI-053", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_program_ir.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1399, 1404, "C12B-BSI-054", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_project_driver.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1405, 1410, "C12B-BSI-055", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_promotion_readiness.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1411, 1416, "C12B-BSI-056", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_repeated_fixed_point.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1417, 1422, "C12B-BSI-057", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_robustness_final.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1423, 1428, "C12B-BSI-058", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_robustness.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1429, 1434, "C12B-BSI-059", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_runtime_bootstrap_contract.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1435, 1440, "C12B-BSI-060", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_runtime_library.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
}};
}

void registerSelfHostBootstrapInfrastructureCatalog049To060Module(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_bootstrap_infrastructure_support::addInfrastructurePendingRange(registry, range);
    }
}
}

