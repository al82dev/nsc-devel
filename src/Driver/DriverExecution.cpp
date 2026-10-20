#include "Internal/DriverInternal.h"

namespace nativesharp::driver
{
using target::BuiltinLayoutKind;

const compilation::ProjectLoadOptions &activeProjectLoadOptions() noexcept
{
    static const compilation::ProjectLoadOptions defaults;
    return activeExecutionScope != nullptr ? activeExecutionScope->projectLoadOptions : defaults;
}

compilation::ProjectLoader &activeProjectLoader() noexcept
{
    static thread_local compilation::ProjectLoader fallback;
    return activeExecutionScope != nullptr ? activeExecutionScope->projectLoader : fallback;
}

// NSC305-P02. Emits the per-code-unit LLVM/cache attribution. Called only when the caller has
// already decided timing reporting is active, so the normal build report remains byte-identical.
void appendCodeUnitTelemetry(std::ostream &output, const llvm_backend::ProjectOopObjectBundle &bundle)
{
    const auto telemetry = bundle.codeUnitTelemetry();
    if (telemetry.empty())
        return;
    for (const llvm_backend::CodeUnitTelemetry &unit : telemetry)
    {
        output << "code-unit " << unit.name << " fingerprint-us=" << unit.fingerprintMicros
               << " lookup-us=" << unit.lookupMicros << " lowering-us=" << unit.loweringMicros
               << " target-us=" << unit.targetMicros << " optimization-us=" << unit.optimizationMicros
               << " emission-us=" << unit.emissionMicros << " verification-us=" << unit.verificationMicros
               << " store-us=" << unit.storeMicros
               << " miss-reason=" << llvm_backend::cacheMissReasonName(unit.missReason) << '\n';
    }
}

void configureProjectExecution(frontend::ProjectCompilation &compilation)
{
    const CompilationJobPolicy *const jobs = activeCompilationJobPolicy();
    compilation.setCompilationJobs(jobs != nullptr ? jobs->effectiveJobs() : 1);
    compilation.setProjectLoadOptions(activeProjectLoadOptions());
    compilation.setCancellationCheck([] { return activeExecutionScope != nullptr && activeExecutionScope->expired(); });
    if (activeExecutionScope != nullptr && activeExecutionScope->reportTiming)
    {
        DriverExecutionScope *const scope = activeExecutionScope;
        compilation.setPhaseTimingSink([scope](std::string_view phase, std::chrono::nanoseconds elapsed)
                                       { scope->recordPhase(phase, elapsed); });
    }
    if (activeExecutionScope != nullptr && activeExecutionScope->reportProgress)
    {
        DriverExecutionScope *const scope = activeExecutionScope;
        compilation.setProgressSink([scope](const basic::CompilationEvent &event) { scope->writeProgress(event); });
    }
#if 0  // INSTRUMENTATION CODE: connect detailed breadcrumbs only in a diagnostic build.
    if (activeExecutionScope != nullptr && activeExecutionScope->genericsTrace)
    {
        DriverExecutionScope* const scope = activeExecutionScope;
        compilation.setDetailedProgressSink(
            [scope](const basic::CompilationEvent& event) {
                scope->writeDetailedProgress(event);
            });
    }
#endif // INSTRUMENTATION CODE
}

std::size_t effectiveCompilationJobs() noexcept
{
    const CompilationJobPolicy *const jobs = activeCompilationJobPolicy();
    return jobs != nullptr ? jobs->effectiveJobs() : 1;
}
} // namespace nativesharp::driver
