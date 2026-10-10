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
    {1297, 1302, "C12B-BSI-037", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_functional_equivalence.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1303, 1308, "C12B-BSI-038", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_generics_aot.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1309, 1314, "C12B-BSI-039", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_image_replay_retired.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1315, 1320, "C12B-BSI-040", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_interop_base.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1321, 1326, "C12B-BSI-041", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_interop_parity.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1327, 1332, "C12B-BSI-042", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_ir.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1333, 1338, "C12B-BSI-043", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_llvm_backend.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1339, 1344, "C12B-BSI-044", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_llvm_c_contract.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1345, 1350, "C12B-BSI-045", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_low_level_parity.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1351, 1356, "C12B-BSI-046", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_module_set.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1357, 1362, "C12B-BSI-047", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_native_text.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1363, 1368, "C12B-BSI-048", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_nsc_next_skeleton.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
}};
}

void registerSelfHostBootstrapInfrastructureCatalog037To048Module(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_bootstrap_infrastructure_support::addInfrastructurePendingRange(registry, range);
    }
}
}

