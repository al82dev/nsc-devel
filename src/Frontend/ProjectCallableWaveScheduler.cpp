#include "nativesharp/Frontend/ProjectCallableWaveScheduler.h"

#include "nativesharp/Driver/CompilationWorkerPool.h"

#include <algorithm>
#include <cassert>
#include <exception>
#include <optional>
#include <string_view>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
using ResultSlot = std::optional<ProjectCallableAnalysisResult>;

std::string failureMessage(
    std::string_view prefix,
    const std::exception_ptr& failure)
{
    if (failure == nullptr) return std::string(prefix);
    try
    {
        std::rethrow_exception(failure);
    }
    catch (const std::exception& exception)
    {
        return std::string(prefix) + ": " + exception.what();
    }
    catch (...)
    {
        return std::string(prefix);
    }
}

std::string wavePhaseName(std::size_t waveOrdinal, std::string_view suffix)
{
    return "method-bodies-wave-" + std::to_string(waveOrdinal) + "-" +
        std::string(suffix);
}

void emitTiming(
    const ProjectCallableWaveScheduler::TimingSink& sink,
    std::size_t waveOrdinal,
    std::string_view suffix,
    std::chrono::steady_clock::time_point started)
{
    if (!sink) return;
    sink(wavePhaseName(waveOrdinal, suffix),
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - started));
}

bool cancellationRequested(
    const ProjectCallableWaveScheduler::CancellationCheck& check)
{
    return check && check();
}

bool publishCompletedSlots(
    std::vector<ResultSlot>& slots,
    std::vector<ProjectCallableAnalysisResult>& results,
    std::string& error)
{
    results.clear();
    for (const ResultSlot& slot : slots)
    {
        if (!slot.has_value())
        {
            error = "callable analysis wave did not complete every scheduled slot";
            return false;
        }
    }
    results.reserve(slots.size());
    for (ResultSlot& slot : slots)
    {
        assert(slot.has_value());
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access): validated above for atomic publish.
        results.push_back(std::move(slot.value()));
    }
    return true;
}

bool runSynchronousWave(
    std::span<const std::size_t> callables,
    std::size_t waveOrdinal,
    const ProjectCallableWaveScheduler::Analyze& analyze,
    const ProjectCallableWaveScheduler::CancellationCheck& cancellationCheck,
    std::vector<ProjectCallableAnalysisResult>& results,
    std::string& error)
{
    std::vector<ResultSlot> slots(callables.size());
    try
    {
        for (std::size_t index = 0; index < callables.size(); ++index)
        {
            if (cancellationRequested(cancellationCheck))
            {
                error = "callable analysis cancelled before completing the wave";
                results.clear();
                return false;
            }
            slots[index].emplace(analyze(callables[index], waveOrdinal));
        }
        if (cancellationRequested(cancellationCheck))
        {
            error = "callable analysis cancelled before publishing the wave";
            results.clear();
            return false;
        }
    }
    catch (const std::exception&)
    {
        error = failureMessage("callable analysis failed", std::current_exception());
        results.clear();
        return false;
    }
    catch (...)
    {
        error = failureMessage("callable analysis failed", std::current_exception());
        results.clear();
        return false;
    }
    return publishCompletedSlots(slots, results, error);
}

bool waitForWave(
    driver::CompilationWorkerPool& pool,
    std::size_t waveOrdinal,
    const ProjectCallableWaveScheduler::CancellationCheck& cancellationCheck,
    const ProjectCallableWaveScheduler::TimingSink& timingSink,
    std::vector<ResultSlot>& slots,
    std::vector<ProjectCallableAnalysisResult>& results,
    std::string& error)
{
    const auto started = timingSink
        ? std::chrono::steady_clock::now()
        : std::chrono::steady_clock::time_point{};
    std::exception_ptr failure;
    try
    {
        pool.wait();
    }
    catch (...)
    {
        failure = std::current_exception();
    }
    emitTiming(timingSink, waveOrdinal, "wait", started);
    if (failure != nullptr)
    {
        error = failureMessage("callable analysis failed", failure);
        results.clear();
        return false;
    }
    if (cancellationRequested(cancellationCheck))
    {
        error = "callable analysis cancelled before publishing the wave";
        results.clear();
        return false;
    }
    return publishCompletedSlots(slots, results, error);
}

std::exception_ptr cancelSubmittedTasks(driver::CompilationWorkerPool& pool) noexcept
{
    pool.requestCancellation();
    try
    {
        pool.wait();
        return nullptr;
    }
    catch (...)
    {
        return std::current_exception();
    }
}

bool runParallelWave(
    std::span<const std::size_t> callables,
    std::size_t waveOrdinal,
    driver::CompilationWorkerPool& pool,
    const ProjectCallableWaveScheduler::Analyze& analyze,
    const ProjectCallableWaveScheduler::CancellationCheck& cancellationCheck,
    const ProjectCallableWaveScheduler::TimingSink& timingSink,
    std::vector<ProjectCallableAnalysisResult>& results,
    std::string& error)
{
    std::vector<ResultSlot> slots(callables.size());
    try
    {
        for (std::size_t index = 0; index < callables.size(); ++index)
        {
            pool.submit([&, index](std::size_t) {
                slots[index].emplace(analyze(callables[index], waveOrdinal));
            });
        }
    }
    catch (...)
    {
        const std::exception_ptr schedulingFailure = std::current_exception();
        const std::exception_ptr cleanupFailure = cancelSubmittedTasks(pool);
        error = failureMessage("callable analysis scheduling failed", schedulingFailure);
        if (cleanupFailure != nullptr)
            error += "; " + failureMessage(
                "callable analysis cleanup failed", cleanupFailure);
        results.clear();
        return false;
    }
    return waitForWave(
        pool, waveOrdinal, cancellationCheck, timingSink, slots, results, error);
}
}

ProjectCallableWaveScheduler::ProjectCallableWaveScheduler(
    std::size_t workerCount) noexcept
    : ProjectCallableWaveScheduler(workerCount, nullptr)
{
}

ProjectCallableWaveScheduler::ProjectCallableWaveScheduler(
    std::size_t workerCount,
    driver::CompilationWorkerPool* workerPool) noexcept
    : workerCount_(std::max<std::size_t>(1, workerCount)),
      pool_(workerPool)
{
    if (pool_ == nullptr && workerCount_ > 1 &&
        driver::currentCompilationWorkerIndex() == driver::kNoCompilationWorker)
    {
        try
        {
            ownedPool_ = std::make_unique<driver::CompilationWorkerPool>(workerCount_);
            pool_ = ownedPool_.get();
        }
        catch (...)
        {
            poolInitializationFailure_ = std::current_exception();
        }
    }
}

ProjectCallableWaveScheduler::~ProjectCallableWaveScheduler() = default;

bool ProjectCallableWaveScheduler::run(
    std::span<const std::size_t> callables,
    std::size_t waveOrdinal,
    const Analyze& analyze,
    const CancellationCheck& cancellationCheck,
    std::vector<ProjectCallableAnalysisResult>& results,
    std::string& error,
    const TimingSink& timingSink)
{
    error.clear();
    results.clear();
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true))
    {
        error = "callable analysis scheduler is already running";
        return false;
    }
    bool succeeded = false;
    const auto started = timingSink
        ? std::chrono::steady_clock::now()
        : std::chrono::steady_clock::time_point{};
    try
    {
        succeeded = runExclusive(
            callables, waveOrdinal, analyze, cancellationCheck,
            results, error, timingSink);
    }
    catch (...)
    {
        error = failureMessage("callable analysis failed", std::current_exception());
        results.clear();
    }
    running_.store(false);
    emitTiming(timingSink, waveOrdinal, "analysis", started);
    return succeeded;
}

bool ProjectCallableWaveScheduler::runExclusive(
    std::span<const std::size_t> callables,
    std::size_t waveOrdinal,
    const Analyze& analyze,
    const CancellationCheck& cancellationCheck,
    std::vector<ProjectCallableAnalysisResult>& results,
    std::string& error,
    const TimingSink& timingSink)
{
    if (!analyze)
    {
        error = "callable analysis callback is empty";
        return false;
    }
    if (poolInitializationFailure_ != nullptr)
    {
        error = failureMessage(
            "callable analysis worker pool initialization failed",
            poolInitializationFailure_);
        return false;
    }
    if (cancellationRequested(cancellationCheck))
    {
        error = "callable analysis cancelled before starting the wave";
        return false;
    }
    if (pool_ == nullptr || callables.size() < 2 ||
        driver::currentCompilationWorkerIndex() != driver::kNoCompilationWorker)
        return runSynchronousWave(
            callables, waveOrdinal, analyze, cancellationCheck, results, error);

    return runParallelWave(
        callables, waveOrdinal, *pool_, analyze,
        cancellationCheck, timingSink, results, error);
}

std::size_t ProjectCallableWaveScheduler::requestedWorkerCount() const noexcept
{
    return workerCount_;
}

std::size_t ProjectCallableWaveScheduler::effectiveWorkerCount() const noexcept
{
    return pool_ != nullptr ? pool_->workerCount() : 1;
}

bool ProjectCallableWaveScheduler::usesParallelWave(
    std::size_t callableCount) const noexcept
{
    return pool_ != nullptr && callableCount >= 2 &&
        driver::currentCompilationWorkerIndex() == driver::kNoCompilationWorker;
}
}
