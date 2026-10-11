#include "DriverTestSupport.h"

#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Driver/CompilerConsole.h"
#include "nativesharp/Driver/CrashBoundary.h"
#include "nativesharp/Driver/DeprecationPolicy.h"
#include "nativesharp/Driver/ProjectCommands.h"

#include <atomic>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace nsc::suite
{
namespace
{
using namespace driver_support;
using namespace nativesharp::driver;

void workerSubmissionContract(TestContext& context, std::size_t variant)
{
    const std::size_t workerCount = variant == 8U ? 16U : 2U;
    CompilationWorkerPool pool(workerCount);
    std::atomic_size_t executed{0U};
    std::atomic_bool validIdentity{true};
    const std::size_t taskCount = variant == 8U ? 4096U : 8U;
    for (std::size_t index = 0; index < taskCount; ++index)
    {
        const std::size_t ordinal = pool.submit([&](std::size_t workerIndex)
        {
            validIdentity = validIdentity && workerIndex < workerCount &&
                currentCompilationWorkerIndex() == workerIndex;
            ++executed;
        });
        if (ordinal != index) validIdentity = false;
    }
    pool.wait();
    context.expect(executed == taskCount && validIdentity &&
            currentCompilationWorkerIndex() == kNoCompilationWorker,
        "worker submission ordinals, execution count, and worker identity are exact");
}

void workerFailureContract(TestContext& context, std::size_t variant)
{
    CompilationWorkerPool pool(2U);
    pool.submit([variant](std::size_t)
    {
        if (variant != 3U) throw std::runtime_error("first failure");
    });
    pool.submit([](std::size_t) { throw std::logic_error("second failure"); });
    std::string message;
    try { pool.wait(); }
    catch (const std::exception& exception) { message = exception.what(); }
    context.expect(message == (variant == 3U ? "second failure" : "first failure"),
        "worker failures are rethrown by the lowest submitted ordinal");
}

void workerCancellationContract(TestContext& context, std::size_t variant)
{
    CompilationWorkerPool pool(1U);
    std::atomic_bool started{false};
    std::atomic_bool release{false};
    std::atomic_size_t executed{0U};
    pool.submit([&](std::size_t)
    {
        started.store(true, std::memory_order_release);
        ++executed;
        while (!release.load(std::memory_order_acquire)) std::this_thread::yield();
    });
    while (!started.load(std::memory_order_acquire)) std::this_thread::yield();
    const std::size_t queued = variant == 8U ? 1000U : 10U;
    for (std::size_t index = 0; index < queued; ++index)
        pool.submit([&](std::size_t) { ++executed; });
    pool.requestCancellation();
    const bool cancellationObserved = pool.cancellationRequested();
    release.store(true, std::memory_order_release);
    pool.wait();
    const bool cancellationClosed = !pool.cancellationRequested();
    pool.submit([&](std::size_t) { ++executed; });
    pool.wait();
    context.expect(cancellationObserved && cancellationClosed && executed == 2U &&
            pool.discardedTaskCount() == queued,
        "cancellation closes after wait and the worker pool accepts a new task");
}

void consoleArgumentsContract(TestContext& context, std::size_t variant)
{
    const std::vector<std::string_view> arguments = variant == 3U ?
        std::vector<std::string_view>{"--language"} : variant == 4U ?
        std::vector<std::string_view>{"--language=fr", "build"} : variant == 5U ?
        std::vector<std::string_view>{"--language=en", "--language=es"} :
        std::vector<std::string_view>{"build", "--language=es", "App.nsproj"};
    const auto parsed = parseCompilerConsoleArguments(arguments);
    if (variant >= 3U && variant <= 5U)
    {
        context.expect(!parsed.error.empty(),
            "missing, unsupported, and repeated language options are rejected");
        return;
    }
    context.expect(parsed.error.empty() && parsed.language == CompilerLanguage::Spanish &&
            parsed.arguments == std::vector<std::string_view>({"build", "App.nsproj"}),
        "language option is removed while all other argument views preserve order");
}

void consoleLocalizationContract(TestContext& context, std::size_t variant)
{
    const CompilerLanguage original = activeCompilerLanguage();
    std::ostringstream banner;
    {
        CompilerLanguageScope scope(variant == 3U ? CompilerLanguage::English :
            CompilerLanguage::Spanish);
        printCompilerBanner(banner);
        const std::string translated = localizeCompilerMessage("compiler failed");
        context.expect(variant == 3U ? translated == "compiler failed" :
                translated == "el compilador falló",
            "active language selects exact console message");
    }
    context.expect(activeCompilerLanguage() == original &&
            contains(banner.str(), variant == 3U ? "NativeSharp Compiler" : "Compilador NativeSharp"),
        "language scope restores thread-local state and banner localization");
}

void crashBoundaryContract(TestContext& context, std::size_t variant)
{
    std::ostringstream error;
    const int result = runWithCrashBoundary([variant]() -> int
    {
        if (variant == 3U) throw std::bad_alloc{};
        if (variant == 4U) throw std::runtime_error("boom");
        if (variant == 5U) throw 17;
        return 42;
    }, error);
    if (variant >= 3U && variant <= 5U)
    {
        context.expect(result == static_cast<int>(nativesharp::ExitCode::InternalError) &&
                contains(error.str(), variant == 3U ? "out of memory" :
                    variant == 4U ? "boom" : "unknown unhandled exception"),
            "crash boundary maps each exception class to stable internal-error text");
        return;
    }
    context.expect(result == 42 && error.str().empty(),
        "crash boundary preserves successful result and emits no diagnostic");
}

void deprecationCatalogContract(TestContext& context, std::size_t variant)
{
    if (variant == 3U)
    {
        context.expect(!parseDeprecationRelease("").has_value() &&
                !parseDeprecationRelease("3.x").has_value() &&
                !parseDeprecationRelease("3").has_value(),
            "malformed release labels are rejected");
        return;
    }
    const auto release = parseDeprecationRelease(variant == 8U ? "4294967295.0" : "3.4");
    const auto features = deprecatedFeatures();
    bool lookups = !features.empty();
    for (const auto& feature : features)
        lookups = lookups && deprecatedFeature(feature.identity) == &feature &&
            deprecatedFeature(feature.issue) == &feature;
    context.expect(release.has_value() && release->major == (variant == 8U ? 4294967295U : 3U) &&
            lookups && deprecatedFeature("does-not-exist") == nullptr &&
            deprecationStateName(DeprecationState::Removed) == "removed",
        "deprecation releases, catalog lookup, and state names are canonical");
}

void deprecationDecisionContract(TestContext& context, std::size_t variant)
{
    DeprecatedFeature feature{"legacy", DeprecationKind::Behavior,
        nativesharp::language::MigrationIssue::NewlyContextualWord, {3U, 0U}, {4U, 0U}};
    const bool suppressed = variant == 3U;
    const bool warningsAsErrors = variant == 4U;
    const DeprecationRelease release = variant == 5U ? DeprecationRelease{4U, 0U} :
        DeprecationRelease{3U, 1U};
    const auto decision = evaluateDeprecation(feature, release, suppressed, warningsAsErrors);
    const auto expected = suppressed ? DeprecationState::Suppressed :
        release.major >= 4U ? DeprecationState::Removed :
        warningsAsErrors ? DeprecationState::Error : DeprecationState::Warning;
    context.expect(decision.feature == &feature && decision.state == expected &&
            deprecationRemovalAllowed(feature, {4U, 0U}, true, false) &&
            !deprecationRemovalAllowed(feature, {3U, 9U}, true, false),
        "deprecation state and minimum removal release follow the policy matrix");
}

void driverDispatchContract(TestContext& context, std::size_t variant)
{
    const std::vector<std::string_view> arguments = variant == 3U ?
        std::vector<std::string_view>{} : variant == 4U ?
        std::vector<std::string_view>{"explain"} :
        std::vector<std::string_view>{"explain", "build"};
    std::ostringstream output;
    std::ostringstream error;
    const auto executor = [](std::span<const std::string_view>, std::ostream&,
        std::ostream&) { return nativesharp::ExitCode::Success; };
    const nativesharp::ExitCode code =
        runProjectCommand(arguments, output, error, executor);
    if (variant == 3U)
    {
        context.expect(code == nativesharp::ExitCode::CommandLineError &&
                output.str().empty() && error.str().empty(),
            "empty command dispatch returns the canonical command-line result");
        return;
    }
    if (variant == 4U)
    {
        context.expect(code == nativesharp::ExitCode::CommandLineError &&
                contains(error.str(), "usage: nsc explain"),
            "missing command operand returns exact command-specific help");
        return;
    }
    context.expect(code == nativesharp::ExitCode::Success &&
            contains(output.str(), "NSEXPLAIN 1.0") && error.str().empty(),
        "valid nested command dispatch publishes its exact result");
}
}

void registerDriverRuntime097To192Module(TestRegistry& registry)
{
    addDriverContract(registry, 13U, "integration", workerSubmissionContract);
    addDriverContract(registry, 14U, "integration", workerFailureContract);
    addDriverContract(registry, 15U, "integration", workerCancellationContract);
    addDriverContract(registry, 16U, "unit", consoleArgumentsContract);
    addDriverContract(registry, 17U, "unit", consoleLocalizationContract);
    addDriverContract(registry, 18U, "integration", crashBoundaryContract);
    addDriverContract(registry, 19U, "unit", deprecationCatalogContract);
    addDriverContract(registry, 20U, "unit", deprecationDecisionContract);
    addInfrastructureContract(registry, 21U, "integration",
        "CF-DRIVER-021 requires a controlled project graph and secret-redaction fixture");
    addInfrastructureContract(registry, 22U, "integration",
        "CF-DRIVER-022 requires transactional diagnostic-bundle fault injection");
    addInfrastructureContract(registry, 23U, "unit",
        "CF-DRIVER-023 has no public option-extraction seam outside Driver.cpp");
    addDriverContract(registry, 24U, "unit", driverDispatchContract);
}
}
