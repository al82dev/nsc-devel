#include "LlvmTestSupport.h"

namespace nsc::suite
{
using namespace llvm_support;

void registerLlvmOopAbiLifetimeModule(TestRegistry& registry)
{
    addInfrastructureContract(registry, 41, "unit", "descriptor-vtable-interface-metadata-ir-oracle");
    addInfrastructureContract(registry, 42, "unit", "rtti-type-test-catch-filter-ir-oracle");
    addInfrastructureContract(registry, 43, "integration", "foreign-callback-exception-boundary-execution");
    addInfrastructureContract(registry, 44, "integration", "runtime-import-loader-and-c-abi-wrapper-fixture");
    addInfrastructureContract(registry, 45, "unit", "parameter-and-return-abi-structural-ir-oracle");
    addInfrastructureContract(registry, 46, "unit", "entry-alloca-local-ownership-flag-ir-oracle");
    addInfrastructureContract(registry, 47, "unit", "windows-seh-cleanup-chain-and-funclet-oracle");
    addInfrastructureContract(registry, 48, "unit", "using-raii-drop-destruction-ir-oracle");
}
}
