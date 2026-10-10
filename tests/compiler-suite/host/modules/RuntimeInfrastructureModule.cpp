#include "RuntimeTestSupport.h"

namespace nsc::suite
{
void registerRuntimeInfrastructureModule(TestRegistry& registry)
{
    using runtime_support::addInfrastructureContract;

    addInfrastructureContract(registry, 11, "unit", "LLVM-builder-death-process-arity-guard");
    addInfrastructureContract(registry, 12, "integration", "linked-Win32-runtime-heap-and-ASAN");
    addInfrastructureContract(registry, 13, "integration", "isolated-SEH-panic-child-process");
    addInfrastructureContract(registry, 14, "integration", "executable-entry-SEH-child-process");
    addInfrastructureContract(registry, 15, "integration", "console-handle-redirection-process-fixture");
    addInfrastructureContract(registry, 16, "integration", "console-input-EOF-process-fixture");
    addInfrastructureContract(registry, 17, "integration", "Win32-clock-runtime-process-fixture");
    addInfrastructureContract(registry, 18, "integration", "exit-path-isolated-child-process");
    addInfrastructureContract(registry, 19, "integration", "isolated-environment-process-fixture");
    addInfrastructureContract(registry, 20, "unit", "LLVM-runtime-string-conversion-harness");
    addInfrastructureContract(registry, 21, "integration", "filesystem-lifecycle-failure-injection");
    addInfrastructureContract(registry, 22, "integration", "filesystem-text-IO-failure-injection");
    addInfrastructureContract(registry, 23, "integration", "bounded-byte-IO-ASAN-and-injection");
    addInfrastructureContract(registry, 24, "integration", "isolated-current-directory-process-fixture");
    addInfrastructureContract(registry, 25, "integration", "child-process-redirection-timeout-fixture");
    addInfrastructureContract(registry, 26, "integration", "command-line-child-process-matrix");
    addInfrastructureContract(registry, 27, "integration", "directory-enumeration-ASAN-fixture");
    addInfrastructureContract(registry, 28, "integration", "thread-delegate-runtime-and-ASAN-harness");
    addInfrastructureContract(registry, 29, "integration", "thread-join-detach-timeout-process-fixture");
    addInfrastructureContract(registry, 30, "integration", "named-mutex-cross-process-fixture");
    addInfrastructureContract(registry, 31, "integration", "monitor-scheduling-and-timeout-harness");
    addInfrastructureContract(registry, 32, "integration", "semaphore-handle-failure-injection");
    addInfrastructureContract(registry, 33, "integration", "named-event-cross-process-fixture");
    addInfrastructureContract(registry, 34, "integration", "isolated-process-metrics-priority-fixture");
    addInfrastructureContract(registry, 35, "integration", "Win32-system-memory-observation-fixture");
    addInfrastructureContract(registry, 36, "integration", "debugger-stack-and-debug-output-process-fixture");
    addInfrastructureContract(registry, 37, "integration", "filesystem-copy-move-failure-injection");
    addInfrastructureContract(registry, 38, "unit", "linked-atomic-runtime-concurrency-and-ASAN");
    addInfrastructureContract(registry, 39, "unit", "interlocked-runtime-concurrency-harness");
    addInfrastructureContract(registry, 40, "unit", "LLVM-half-helper-execution-and-bit-oracle");
    addInfrastructureContract(registry, 41, "integration", "exception-TLS-multithread-process-harness");
    addInfrastructureContract(registry, 42, "integration", "typed-SEH-payload-isolated-process");
    addInfrastructureContract(registry, 43, "unit", "LLVM-runtime-builder-routing-fixture");
    addInfrastructureContract(registry, 44, "unit", "LLVM-entry-module-structural-fixture");
    addInfrastructureContract(registry, 45, "integration", "LLVM-runtime-module-assembly-fixture");
    addInfrastructureContract(registry, 46, "unit", "LLVM-dynamic-library-anchor-fixture");
    addInfrastructureContract(registry, 47, "unit", "LLVM-Win32-import-signature-catalogue");
    addInfrastructureContract(registry, 48, "integration", "LLVM-verifier-and-runtime-ABI-integration");
}
}
