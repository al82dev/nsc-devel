#include "ColdLlvmTrace.h"

#include "nativesharp/LLVM/CompilationArtifactCache.h"
#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"

#include <algorithm>
#include <string>

namespace nativesharp::llvm_backend
{
ColdLlvmTrace::ColdLlvmTrace(
    std::string_view codegenIdentity,
    std::string_view projectFingerprint,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimization,
    std::size_t moduleCount,
    bool partitioned,
    bool preLlvmCache,
    CachePruneTelemetry pruneTelemetry,
    const ColdLlvmPreambleTelemetry& preamble)
    : output_(R"(A:\temp\nsc-llvm-code-unit-timing.log)",
          std::ios::binary | std::ios::trunc),
      preamble_(preamble)
{
    if (!output_) return;
    output_ << "NSC305-L06 LLVM CODE UNIT INSTRUMENTATION version=4\n"
            << "codegen-id=" << codegenIdentity
            << " project=" << projectFingerprint
            << " debug=" << debugInformationKindName(debugInformation)
            << " optimization=" << llvmOptimizationLevelName(optimization)
            << " modules=" << moduleCount
            << " partitioned=" << (partitioned ? 1 : 0)
            << " pre-llvm-cache=" << (preLlvmCache ? 1 : 0)
            << " prune-performed=" << (pruneTelemetry.performed ? 1 : 0)
            << " prune-us=" << pruneTelemetry.elapsedMicros
            << " prune-entries=" << pruneTelemetry.inspectedEntries
            << " prune-bytes=" << pruneTelemetry.inspectedBytes
            << " prune-evicted-entries=" << pruneTelemetry.evictedEntries
            << " prune-evicted-bytes=" << pruneTelemetry.evictedBytes << '\n';
    output_.flush();
}

ColdLlvmTrace::~ColdLlvmTrace()
{
    const std::lock_guard<std::mutex> lock(mutex_);
    if (!output_) return;
    output_ << "preamble"
            << " reachability-us=" << preamble_.reachabilityMicros
            << " partition-key-us=" << preamble_.partitionKeyMicros
            << " partition-restore-us=" << preamble_.partitionRestoreMicros
            << " partition-build-us=" << preamble_.partitionBuildMicros
            << " coordinator-closure-us=" << preamble_.coordinatorClosureMicros
            << " identity-index-build-us=" << preamble_.identityIndexBuildMicros
            << " line-table-identity-us=" << preamble_.lineTableIdentityMicros
            << " module-key-compose-us=" << preamble_.moduleKeyComposeMicros
            << " static-field-mask-us=" << preamble_.staticFieldMaskMicros
            << " worker-wall-us=" << preamble_.workerWallMicros
            << " plan-store-us=" << preamble_.planStoreMicros << '\n';
    output_.flush();
}

void ColdLlvmTrace::record(
    const CodeUnitTelemetry& telemetry,
    bool hit,
    bool miss,
    std::string_view error)
{
    const std::lock_guard<std::mutex> lock(mutex_);
    if (!output_) return;
    std::string oneLineError(error);
    std::ranges::replace(oneLineError, '\r', ' ');
    std::ranges::replace(oneLineError, '\n', ' ');
    output_ << "code-unit " << telemetry.name
            << " hit=" << (hit ? 1 : 0)
            << " miss=" << (miss ? 1 : 0)
            << " miss-reason=" << cacheMissReasonName(telemetry.missReason)
            << " definitions=" << telemetry.definitionCount
            << " declarations=" << telemetry.declarationCount
            << " static-fields=" << telemetry.staticFieldCount
            << " static-field-mask-us=" << telemetry.staticFieldMaskMicros
            << " fingerprint-us=" << telemetry.fingerprintMicros
            << " lookup-us=" << telemetry.lookupMicros
            << " lowering-us=" << telemetry.loweringMicros
            << " target-us=" << telemetry.targetMicros
            << " optimization-us=" << telemetry.optimizationMicros
            << " emission-us=" << telemetry.emissionMicros
            << " verification-us=" << telemetry.verificationMicros
            << " store-us=" << telemetry.storeMicros;
    if (!oneLineError.empty()) output_ << " error=" << oneLineError;
    output_ << '\n';
    output_.flush();
}
}
