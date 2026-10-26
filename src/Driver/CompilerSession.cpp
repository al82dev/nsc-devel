#include "Internal/DriverInternal.h"

namespace nativesharp::driver
{
using target::BuiltinLayoutKind;

#if 0  // INSTRUMENTATION CODE: enable only for an explicitly authorized diagnostic build.
constexpr char kTemporaryDefaultGenericsTracePath[] =
    R"(A:\temp\nsc-generics-trace.log)";
#endif // INSTRUMENTATION CODE

#if 0  // INSTRUMENTATION CODE: exact file-open effects for the parked CLI parser.
ExitCode openCompilerGenericsTrace(
    const std::filesystem::path& path,
    std::string_view displayPath,
    bool automatic,
    std::ostream& error)
{
    const std::filesystem::path tracePath = automatic
        ? std::filesystem::path(kTemporaryDefaultGenericsTracePath)
        : path;
    if (activeExecutionScope != nullptr && activeExecutionScope->openGenericsTrace(tracePath))
        return ExitCode::Success;
    if (automatic)
    {
        error << "nsc: error: could not open temporary generics trace file '"
              << kTemporaryDefaultGenericsTracePath << "'\n";
        return ExitCode::CompilationError;
    }
    error << "nsc: error: could not open generics trace file '"
          << displayPath << "'\n";
    return ExitCode::CommandLineError;
}
#endif // INSTRUMENTATION CODE

ExitCode parseAndConfigureInvocation(std::span<const std::string_view> arguments, DriverExecutionScope &executionScope,
                                     CompilationJobPolicy &jobPolicy, CompilerInvocation &invocation,
                                     std::ostream &error)
{
    const ExitCode result = parseCompilerInvocation(arguments, executionScope.projectLoader, invocation, error);
    executionScope.reportTiming = invocation.reportTiming;
    executionScope.reportProgress = invocation.reportProgress;
    executionScope.validateSystemImports = invocation.validateSystemImports;
    executionScope.progressFormat = invocation.progressFormat;
    executionScope.projectLoadOptions = invocation.projectLoadOptions;
    if (invocation.timeoutMilliseconds.has_value())
    {
        executionScope.deadline = executionScope.started + std::chrono::milliseconds(*invocation.timeoutMilliseconds);
    }
    if (invocation.optimizationBudgetMilliseconds.has_value())
        executionScope.o2Budget.setBudgetMilliseconds(*invocation.optimizationBudgetMilliseconds);
    if (invocation.requestedJobs.has_value())
        jobPolicy.setRequestedJobs(*invocation.requestedJobs);
    if (invocation.reservedCores.has_value())
        jobPolicy.setReservedCores(*invocation.reservedCores);
    executionScope.compilationLifetime.setWorkerCount(jobPolicy.effectiveJobs());
    return result;
}

void installAndReportFloatingPoint(const CompilerInvocation &invocation,
                                   llvm_backend::FloatingPointModeScope &floatingPointModeScope, std::ostream &output)
{
    floatingPointModeScope.set(invocation.floatingPointMode);
    floatingPointModeScope.setFmaContractPolicy(invocation.fmaContractPolicy);
    floatingPointModeScope.setRoundingMode(invocation.floatingPointRoundingMode);
    floatingPointModeScope.setDenormalMode(invocation.floatingPointDenormalMode);
    if (invocation.floatingPointModeSeen)
        output << "floating-point-mode: " << llvm_backend::floatingPointModeName(invocation.floatingPointMode) << '\n';
    if (invocation.fmaContractPolicySeen)
        output << "fma-contract-policy: " << llvm_backend::fmaContractPolicyName(invocation.fmaContractPolicy) << '\n';
    if (invocation.floatingPointRoundingModeSeen)
        output << "floating-point-rounding: "
               << llvm_backend::floatingPointRoundingModeName(invocation.floatingPointRoundingMode) << '\n';
    if (invocation.floatingPointDenormalModeSeen)
        output << "floating-point-denormal: "
               << llvm_backend::floatingPointDenormalModeName(invocation.floatingPointDenormalMode) << '\n';
}

ExitCode pruneInvocationArtifactCache(const CompilerInvocation &invocation,
                                      llvm_backend::CompilationArtifactCache &artifactCache, std::ostream &output,
                                      std::ostream &error)
{
    if (invocation.artifactCacheRoot.has_value())
        artifactCache.setRoot(*invocation.artifactCacheRoot);
    if (!invocation.artifactCacheMaximumSeen && !invocation.defaultArtifactCache)
        return ExitCode::Success;
    std::size_t evictedEntries = 0;
    std::uintmax_t evictedBytes = 0;
    std::string pruneError;
    if (!artifactCache.prune(invocation.artifactCacheMaximumBytes, evictedEntries, evictedBytes, pruneError))
    {
        error << "nsc: error: artifact-cache eviction failed: " << pruneError << '\n';
        return ExitCode::CompilationError;
    }
    output << "artifact-cache-evicted-entries: " << evictedEntries << '\n'
           << "artifact-cache-evicted-bytes: " << evictedBytes << '\n';
    return ExitCode::Success;
}

ExitCode validateInvocationAndCache(CompilerInvocation &invocation,
                                    llvm_backend::CompilationArtifactCache &artifactCache, std::ostream &output,
                                    std::ostream &error)
{
    if (invocation.arguments.empty())
    {
        error << "nsc: error: compilation options require an action or input file\n";
        return ExitCode::CommandLineError;
    }
    ExitCode result = validateCompilerInvocation(invocation, error);
    if (result != ExitCode::Success)
        return result;
    result = pruneInvocationArtifactCache(invocation, artifactCache, output, error);
    if (result != ExitCode::Success)
        return result;
    return validateCompilerLinkOptions(invocation, error);
}

void reportCompilationProfile(const CompilerInvocation &invocation, std::ostream &output)
{
    if (!invocation.compilationProfile.has_value())
        return;
    output << "compilation-profile: " << compilationProfileName(*invocation.compilationProfile) << '\n'
           << "profile-optimization: " << llvm_backend::llvmOptimizationLevelName(invocation.optimizationLevel) << '\n'
           << "profile-debug-information: " << llvm_backend::debugInformationKindName(invocation.debugInformation)
           << '\n';
}

ExitCode executeCompilerSession(const Driver &driver, std::span<const std::string_view> arguments,
                                DriverExecutionScope &executionScope, CompilationJobPolicy &jobPolicy,
                                llvm_backend::CompilationArtifactCache &artifactCache,
                                llvm_backend::FloatingPointModeScope &floatingPointModeScope, std::ostream &output,
                                std::ostream &error)
{
    if (arguments.empty())
    {
        CompilerActionDispatcher::showHelp(output);
        return ExitCode::Success;
    }
    CompilerInvocation invocation;
    ExitCode result = parseAndConfigureInvocation(arguments, executionScope, jobPolicy, invocation, error);
    if (result != ExitCode::Success)
        return result;
    installAndReportFloatingPoint(invocation, floatingPointModeScope, output);
    result = validateInvocationAndCache(invocation, artifactCache, output, error);
    if (result != ExitCode::Success)
        return result;
    reportCompilationProfile(invocation, output);
    return CompilerActionDispatcher(driver, invocation.arguments, invocation, jobPolicy,
                                    executionScope.projectLoadOptions, kCompilerStage, output, error)
        .dispatch();
}

ExitCode runCompilerSession(const Driver &driver, std::span<const std::string_view> arguments, std::ostream &output,
                            std::ostream &error,
                            compilation::CompilationInputManifest *preparedInputManifest)
{
    DriverExecutionScope executionScope(output, error);
    executionScope.preparedInputManifest = preparedInputManifest;
    llvm_backend::LlvmOptimizationBudgetScope optimizationBudgetScope(executionScope.o2Budget);
    CompilationJobPolicy jobPolicy;
    CompilationJobPolicyScope jobPolicyScope(jobPolicy);
    llvm_backend::CompilationArtifactCache artifactCache;
    artifactCache.setCodegenIdentity(std::string(llvm_backend::kCodegenCacheIdentity));
    llvm_backend::CompilationArtifactCacheScope artifactCacheScope(artifactCache);
    llvm_backend::FloatingPointModeScope floatingPointModeScope;
    DriverExecutionScope *previousScope = activeExecutionScope;
    activeExecutionScope = &executionScope;
    try
    {
        const ExitCode result = executeCompilerSession(driver, arguments, executionScope, jobPolicy, artifactCache,
                                                       floatingPointModeScope, output, error);
        activeExecutionScope = previousScope;
        return result;
    }
    catch (...)
    {
        activeExecutionScope = previousScope;
        throw;
    }
}
} // namespace nativesharp::driver
