#include "nativesharp/Frontend/ProjectCallableWaveScheduler.h"

#include "nativesharp/Driver/CompilationWorkerPool.h"

#include <algorithm>
#include <exception>
#include <optional>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
using ResultSlot = std::optional<ProjectCallableAnalysisResult>;

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
        results.push_back(std::move(*slot));
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
    catch (const std::exception& exception)
    {
        error = "callable analysis failed: " + std::string(exception.what());
        results.clear();
        return false;
    }
    catch (...)
    {
        error = "callable analysis failed";
        results.clear();
        return false;
    }
    return publishCompletedSlots(slots, results, error);
}

bool waitForWave(
    driver::CompilationWorkerPool& pool,
    const ProjectCallableWaveScheduler::CancellationCheck& cancellationCheck,
    std::vector<ResultSlot>& slots,
    std::vector<ProjectCallableAnalysisResult>& results,
    std::string& error)
{
    try
    {
        pool.wait();
        if (cancellationRequested(cancellationCheck))
        {
            error = "callable analysis cancelled before publishing the wave";
            results.clear();
            return false;
        }
    }
    catch (const std::exception& exception)
    {
        error = "parallel callable analysis failed: " + std::string(exception.what());
        results.clear();
        return false;
    }
    catch (...)
    {
        error = "parallel callable analysis failed";
        results.clear();
        return false;
    }
    return publishCompletedSlots(slots, results, error);
}

void cancelSubmittedTasks(driver::CompilationWorkerPool& pool) noexcept
{
    pool.requestCancellation();
    try
    {
        pool.wait();
    }
    catch (...)
    {
    }
}

bool runParallelWave(
    std::span<const std::size_t> callables,
    std::size_t waveOrdinal,
    driver::CompilationWorkerPool& pool,
    const ProjectCallableWaveScheduler::Analyze& analyze,
    const ProjectCallableWaveScheduler::CancellationCheck& cancellationCheck,
    std::vector<ProjectCallableAnalysisResult>& results,
    std::string& error)
{
    try
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
            cancelSubmittedTasks(pool);
            throw;
        }
        return waitForWave(
            pool, cancellationCheck, slots, results, error);
    }
    catch (const std::exception& exception)
    {
        error = "parallel callable scheduling failed: " + std::string(exception.what());
    }
    catch (...)
    {
        error = "parallel callable scheduling failed";
    }
    results.clear();
    return false;
}
}

ProjectCallableWaveScheduler::ProjectCallableWaveScheduler(
    std::size_t workerCount) noexcept
    : workerCount_(std::max<std::size_t>(1, workerCount))
{
    if (workerCount_ > 1 &&
        driver::currentCompilationWorkerIndex() == driver::kNoCompilationWorker)
    {
        try
        {
            pool_ = std::make_unique<driver::CompilationWorkerPool>(workerCount_);
        }
        catch (...)
        {
            poolInitializationFailed_ = true;
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
    std::string& error) const
{
    error.clear();
    results.clear();
    if (!analyze)
    {
        error = "callable analysis callback is empty";
        return false;
    }
    if (poolInitializationFailed_)
    {
        error = "callable analysis worker pool initialization failed";
        return false;
    }
    if (pool_ == nullptr || callables.size() < 2)
        return runSynchronousWave(
            callables, waveOrdinal, analyze, cancellationCheck, results, error);

    return runParallelWave(
        callables, waveOrdinal, *pool_, analyze,
        cancellationCheck, results, error);
}

std::size_t ProjectCallableWaveScheduler::requestedWorkerCount() const noexcept
{
    return workerCount_;
}
}
