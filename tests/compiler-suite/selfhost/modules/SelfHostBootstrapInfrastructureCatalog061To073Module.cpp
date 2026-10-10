#include "SelfHostBootstrapInfrastructureTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using selfhost_bootstrap_infrastructure_support::PendingContractRange;

constexpr std::string_view kInfrastructureRequirement =
    "SelfHost.Bootstrap.Infrastructure requires nsc0, nsc1, nsc2, NSLink, LLVM, Python, and the self-host contract producers before the typed bootstrap oracle can be executed";

constexpr std::array<PendingContractRange, 13> kContracts{{
    {1441, 1446, "C12B-BSI-061", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_self_hosted_generation.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1447, 1452, "C12B-BSI-062", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_semantics.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1453, 1458, "C12B-BSI-063", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_source_bool_void.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1459, 1464, "C12B-BSI-064", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_source_control_flow.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1465, 1470, "C12B-BSI-065", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_source_derived_backend.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1471, 1476, "C12B-BSI-066", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_source_expressions.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1477, 1482, "C12B-BSI-067", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_source_floating_scalars.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1483, 1488, "C12B-BSI-068", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_source_functions.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1489, 1494, "C12B-BSI-069", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_source_graph.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1495, 1500, "C12B-BSI-070", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_source_integer_widths.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1501, 1506, "C12B-BSI-071", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_source_loop_control.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1507, 1512, "C12B-BSI-072", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_source_pointers.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
    {1513, 1518, "C12B-BSI-073", "BOOTSTRAP_VERIFIER",
     "selfhost/bootstrap/verify_specialized_code.py",
     "reporte estructurado, hashes, etapas, tiempos y código de salida", kInfrastructureRequirement},
}};
}

void registerSelfHostBootstrapInfrastructureCatalog061To073Module(TestRegistry& registry)
{
    for (const PendingContractRange& range : kContracts)
    {
        selfhost_bootstrap_infrastructure_support::addInfrastructurePendingRange(registry, range);
    }
}
}

