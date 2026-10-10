#include "LlvmTestSupport.h"

namespace nsc::suite
{
using namespace llvm_support;

void registerLlvmOopRuntimeModule(TestRegistry& registry)
{
    addInfrastructureContract(registry, 33, "unit", "debug-path-normalization-windows-case-matrix");
    addInfrastructureContract(registry, 34, "integration", "oop-lowering-request-and-partition-ownership-fixture");
    addInfrastructureContract(registry, 35, "unit", "llvm-23-debug-compile-unit-file-subprogram-oracle");
    addInfrastructureContract(registry, 36, "unit", "llvm-23-debug-physical-type-layout-oracle");
    addInfrastructureContract(registry, 37, "unit", "llvm-23-debug-variable-and-scope-oracle");
    addInfrastructureContract(registry, 38, "unit", "complete-native-runtime-hook-metadata-corpus");
    addInfrastructureContract(registry, 39, "unit", "static-field-accessor-and-type-initializer-ir-oracle");
    addInfrastructureContract(registry, 40, "unit", "drop-and-destruction-declaration-ir-oracle");
}
}
