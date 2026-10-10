#include "InteropTestSupport.h"

namespace nsc::suite
{
using namespace interop_support;

void registerInteropCHeaderModule(TestRegistry& registry)
{
    addInfrastructureContract(registry, 1, "unit", "c-header-project-context-for-private-name-sanitizer");
    addInfrastructureContract(registry, 2, "unit", "c-abi-type-and-pointer-decomposition-project-fixture");
    addInfrastructureContract(registry, 3, "integration", "reachable-host-type-project-closure");
    addInfrastructureContract(registry, 4, "unit", "opaque-and-forward-declaration-project-fixture");
    addInfrastructureContract(registry, 5, "unit", "delegate-c-declaration-project-fixture");
    addInfrastructureContract(registry, 6, "integration", "struct-union-physical-layout-project-fixture");
    addInfrastructureContract(registry, 7, "integration", "anonymous-aggregate-array-bitfield-project-fixture");
    addInfrastructureContract(registry, 8, "integration", "c-abi-export-project-fixture");
}
}
