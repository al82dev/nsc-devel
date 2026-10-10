#include "DriverTestSupport.h"

namespace nsc::suite
{
using namespace driver_support;

void registerDriverPipeline193To344Module(TestRegistry& registry)
{
    addInfrastructureContract(registry, 25U, "integration",
        "CF-DRIVER-025 requires a controlled Driver execution scope, clock, deadline, and progress sink");
    addInfrastructureContract(registry, 26U, "integration",
        "CF-DRIVER-026 requires the complete project phase ladder and diagnostic fixture graph");
    addInfrastructureContract(registry, 27U, "integration",
        "CF-DRIVER-027 requires isolated frontend dump and verifier product fixtures");
    addInfrastructureContract(registry, 28U, "integration",
        "CF-DRIVER-028 requires LLVM object and assembly generation infrastructure");
    addInfrastructureContract(registry, 29U, "integration",
        "CF-DRIVER-029 requires NSLink and a controlled executable publication root");
    addInfrastructureContract(registry, 30U, "integration",
        "CF-DRIVER-030 requires project OOP generation and transactional artifact fault injection");
    addInfrastructureContract(registry, 31U, "integration",
        "CF-DRIVER-031 requires entry selection, runtime closure, NSLink, and publication fixtures");
    addInfrastructureContract(registry, 32U, "integration",
        "CF-DRIVER-032 requires archive production plus metadata transaction infrastructure");
    addInfrastructureContract(registry, 33U, "integration",
        "CF-DRIVER-033 requires DLL export, import-library, runtime, and publication infrastructure");
    addInfrastructureContract(registry, 34U, "unit",
        "CF-DRIVER-034 metadata-key conversion is private to Driver.cpp and has no isolated public seam");
    addInfrastructureContract(registry, 35U, "integration",
        "CF-DRIVER-035 requires a parsed public-declaration project and metadata source verifier");
    addInfrastructureContract(registry, 36U, "integration",
        "CF-DRIVER-036 requires generic template materialization through the full frontend");
    addInfrastructureContract(registry, 37U, "integration",
        "CF-DRIVER-037 requires multi-project metadata closure and precompiled dependency fixtures");
    addInfrastructureContract(registry, 38U, "integration",
        "CF-DRIVER-038 requires library-reference resolution roots and binary metadata fixtures");
    addInfrastructureContract(registry, 39U, "integration",
        "CF-DRIVER-039 requires runtime DLL closure and publication fixtures");
    addInfrastructureContract(registry, 40U, "integration",
        "CF-DRIVER-040 requires public header and NSMETA generation from a complete project");
    addInfrastructureContract(registry, 41U, "integration",
        "CF-DRIVER-041 requires linked metadata, ABI, and API surface artifact fixtures");
    addInfrastructureContract(registry, 42U, "integration",
        "CF-DRIVER-042 requires paired metadata revisions for API and ABI differential analysis");
    addInfrastructureContract(registry, 43U, "integration",
        "CF-DRIVER-043 requires archive, DLL, PDB, runtime, and public-source artifact sets");
}
}
