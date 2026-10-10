#include "LlvmTestSupport.h"

namespace nsc::suite
{
using namespace llvm_support;

void registerLlvmGenericPartitionModule(TestRegistry& registry)
{
    addInfrastructureContract(registry, 25, "unit", "llvm-23-arithmetic-comparison-shift-division-ir-oracle");
    addInfrastructureContract(registry, 26, "unit", "llvm-23-conversion-pointer-aggregate-ir-oracle");
    addInfrastructureContract(registry, 27, "unit", "llvm-23-guard-trap-bounds-allocation-ir-oracle");
    addInfrastructureContract(registry, 28, "integration", "project-mir-partition-eligibility-fixture");
    addInfrastructureContract(registry, 29, "integration", "balanced-cost-and-declaration-closure-oracle");
    addInfrastructureContract(registry, 30, "integration", "stable-incremental-hash-determinism-corpus");
    addInfrastructureContract(registry, 31, "unit", "llvm-23-atomic-ordering-ir-oracle");
    addInfrastructureContract(registry, 32, "unit", "llvm-23-floating-policy-ir-oracle");
}
}
