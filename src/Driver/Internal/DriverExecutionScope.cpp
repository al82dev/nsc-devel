#include "DriverExecutionScope.h"

#include "nativesharp/Basic/Version.h"
#include "nativesharp/Driver/CompilerConsole.h"

namespace nativesharp::driver
{
thread_local DriverExecutionScope *activeExecutionScope = nullptr;

DriverExecutionScope::DriverExecutionScope(std::ostream &destination, std::ostream &diagnosticDestination)
    : output(destination), error(diagnosticDestination), started(std::chrono::steady_clock::now())
{
}

DriverExecutionScope::~DriverExecutionScope()
{
    compilationLifetime.reset();
    if (o2Budget.invocations() != 0)
    {
        if (reportTiming)
            output << o2Budget.report() << '\n';
        if (o2Budget.exceeded())
        {
            error << compilerText("nsc: warning: O2 optimization budget exceeded; ",
                                  "nsc: advertencia: se excedió el presupuesto de optimización O2; ")
                  << o2Budget.report() << '\n';
        }
    }
    if (!reportTiming)
        return;
    for (const DriverPhaseTiming &timing : phaseTimings)
    {
        output << "timing phase=" << timing.phase
               << " us=" << std::chrono::duration_cast<std::chrono::microseconds>(timing.elapsed).count()
               << " peak_working_set_bytes=" << timing.peakWorkingSetBytes << '\n';
    }
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
    output << "timing total_ms=" << elapsed << " peak_working_set_bytes=" << currentProcessPeakWorkingSetBytes()
           << '\n';
    const compilation::ProjectLoaderStatistics projectStatistics = projectLoader.statistics();
    output << "timing project_descriptor_file_reads=" << projectStatistics.descriptorFileReads
           << " project_descriptor_cache_hits=" << projectStatistics.descriptorCacheHits << '\n';
}

void DriverExecutionScope::writeProgress(const basic::CompilationEvent &event)
{
    if (!reportProgress)
        return;
    const std::lock_guard<std::mutex> lock(progressMutex);
    basic::CompilationEvent renderedEvent = event;
    if (progressFormat == ProgressFormat::Files &&
        event.kind == basic::CompilationEventKind::WorkFinished && event.phase == "syntax-read")
    {
        renderedEvent.current = ++reportedSourceCount;
    }
    const std::string rendered = renderCompilationEvent(renderedEvent, progressFormat);
    if (rendered.empty())
        return;
    output << rendered << '\n';
    output.flush();
}

#if 0  // INSTRUMENTATION CODE: file-backed generics breadcrumbs.
bool DriverExecutionScope::openGenericsTrace(const std::filesystem::path& path)
{
    genericsTrace.open(path, std::ios::binary | std::ios::trunc);
    if (!genericsTrace) return false;
    genericsTrace << "nsc-generics-trace version=1 build=" << kCompilerBuildId << '\n';
    genericsTrace.flush();
    return static_cast<bool>(genericsTrace);
}

void DriverExecutionScope::writeDetailedProgress(const basic::CompilationEvent& event)
{
    const std::lock_guard<std::mutex> lock(genericsTraceMutex);
    if (!genericsTrace) return;
    genericsTrace << ++genericsTraceSequence << '\t'
                  << renderCompilationEvent(event, ProgressFormat::Text) << '\n';
    genericsTrace.flush();
}
#endif // INSTRUMENTATION CODE

void DriverExecutionScope::recordPhase(std::string_view phase, std::chrono::nanoseconds elapsed)
{
    if (!reportTiming)
        return;
    phaseTimings.push_back({std::string(phase), elapsed, currentProcessPeakWorkingSetBytes()});
}

bool DriverExecutionScope::expired() const noexcept
{
    return deadline.has_value() && std::chrono::steady_clock::now() >= *deadline;
}
} // namespace nativesharp::driver
