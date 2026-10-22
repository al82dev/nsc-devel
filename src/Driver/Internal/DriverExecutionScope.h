#pragma once

#include "DriverPhaseTiming.h"

#include "nativesharp/Basic/CompilationEvent.h"
#include "nativesharp/Compilation/ProjectLoader.h"
#include "nativesharp/Driver/CompilationTelemetry.h"
#include "nativesharp/Frontend/ProjectCompilationLifetime.h"
#include "nativesharp/LLVM/LlvmOptimizationBudgetTracker.h"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <optional>
#include <ostream>
#include <string_view>
#include <vector>

namespace nativesharp::compilation
{
class CompilationInputManifest;
}

namespace nativesharp::driver
{
struct DriverExecutionScope final
{
    DriverExecutionScope(std::ostream &destination, std::ostream &diagnosticDestination);
    ~DriverExecutionScope();

    void writeProgress(const basic::CompilationEvent &event);
#if 0  // INSTRUMENTATION CODE: file-backed generics breadcrumbs.
    bool openGenericsTrace(const std::filesystem::path& path);
    void writeDetailedProgress(const basic::CompilationEvent& event);
#endif // INSTRUMENTATION CODE
    void recordPhase(std::string_view phase, std::chrono::nanoseconds elapsed);
    bool expired() const noexcept;

    std::ostream &output;
    std::ostream &error;
    std::chrono::steady_clock::time_point started;
    std::mutex progressMutex;
#if 0  // INSTRUMENTATION CODE
    std::mutex genericsTraceMutex;
    std::ofstream genericsTrace;
    std::uint64_t genericsTraceSequence = 0;
#endif // INSTRUMENTATION CODE
    std::optional<std::chrono::steady_clock::time_point> deadline;
    std::vector<DriverPhaseTiming> phaseTimings;
    llvm_backend::LlvmOptimizationBudgetTracker o2Budget;
    compilation::ProjectLoader projectLoader;
    compilation::ProjectLoadOptions projectLoadOptions;
    compilation::CompilationInputManifest* preparedInputManifest = nullptr;
    frontend::ProjectCompilationLifetime compilationLifetime;
    bool reportTiming = false;
    bool reportProgress = false;
    ProgressFormat progressFormat = ProgressFormat::Text;
};

extern thread_local DriverExecutionScope *activeExecutionScope;
} // namespace nativesharp::driver
