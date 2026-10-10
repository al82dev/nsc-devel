#include "LlvmTestSupport.h"

namespace nsc::suite
{
using namespace llvm_support;

void registerLlvmOopControlExpressionsModule(TestRegistry& registry)
{
    addInfrastructureContract(registry, 49, "unit", "structured-exit-cleanup-selection-oracle");
    addInfrastructureContract(registry, 50, "unit", "windows-seh-try-catch-finally-transfer-oracle");
    addInfrastructureContract(registry, 51, "unit", "target-specific-inline-assembly-oracle");
    addInfrastructureContract(registry, 52, "unit", "statement-loop-switch-control-flow-ir-oracle");
    addInfrastructureContract(registry, 53, "unit", "oop-expression-dispatch-runtime-intrinsic-corpus");
    addInfrastructureContract(registry, 54, "unit", "capture-cell-retain-release-exception-oracle");
    addInfrastructureContract(registry, 55, "unit", "closure-lambda-captured-state-ir-oracle");
    addInfrastructureContract(registry, 56, "unit", "delegate-event-invocation-abi-oracle");
}
}
