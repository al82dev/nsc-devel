#include "LlvmTestSupport.h"

namespace nsc::suite
{
using namespace llvm_support;

void registerLlvmOopDataObjectsModule(TestRegistry& registry)
{
    addInfrastructureContract(registry, 57, "unit", "aggregate-equality-hash-and-type-name-ir-oracle");
    addInfrastructureContract(registry, 58, "unit", "native-string-runtime-operation-execution-fixture");
    addInfrastructureContract(registry, 59, "unit", "rectangular-array-range-slice-ir-and-execution-oracle");
    addInfrastructureContract(registry, 60, "unit", "cast-match-coalescing-result-propagation-ir-oracle");
    addInfrastructureContract(registry, 61, "unit", "call-permutation-must-tail-dynamic-dispatch-oracle");
    addInfrastructureContract(registry, 62, "unit", "construction-destruction-bitfield-type-abi-oracle");
    addInfrastructureContract(registry, 63, "integration", "physical-mir-v4-identity-golden-corpus");
    addInfrastructureContract(registry, 64, "integration", "parallel-object-cache-and-thinlto-fixture");
}
}
