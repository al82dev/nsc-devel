#pragma once

#include "ColdLlvmPreambleTelemetry.h"

#include "nativesharp/LLVM/CachePruneTelemetry.h"
#include "nativesharp/LLVM/CodeUnitTelemetry.h"
#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"

#include <cstddef>
#include <fstream>
#include <mutex>
#include <optional>
#include <string_view>

namespace nativesharp::llvm_backend
{
// INSTRUMENTATION CODE: NSC305-L06. Temporary, file-backed attribution for GUI builds that
// cannot add --time. Keep this marker until the real-project cold LLVM bottleneck is corrected.
class ColdLlvmTrace final
{
public:
    ColdLlvmTrace(
        std::string_view codegenIdentity,
        std::string_view projectFingerprint,
        DebugInformationKind debugInformation,
        LlvmOptimizationLevel optimization,
        std::size_t moduleCount,
        bool partitioned,
        bool preLlvmCache,
        CachePruneTelemetry pruneTelemetry,
        const ColdLlvmPreambleTelemetry& preamble);

    ~ColdLlvmTrace();

    void record(
        const CodeUnitTelemetry& telemetry,
        bool hit,
        bool miss,
        std::string_view error);

private:
    std::mutex mutex_;
    std::optional<std::ofstream> output_;
    const ColdLlvmPreambleTelemetry& preamble_;
};
}
