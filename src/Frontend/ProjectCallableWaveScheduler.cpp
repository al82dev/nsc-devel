#include "nativesharp/Frontend/ProjectCallableWaveScheduler.h"

#include "nativesharp/Driver/CompilationWorkerPool.h"

#include <algorithm>
#include <atomic>
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
    results.reserve(slots.size());
    for (ResultSlot& slot : slots)
    {
        if (!slot.has_value())
        {
            error = "callable analysis wave did not complete every scheduled slot";
            results.clear();
            return false;
        }
        results.push_back(std::move(*slot));
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
    std::atomic_bool& externallyCancelled,
    std::vector<ResultSlot>& slots,
    std::vector<ProjectCallableAnalysisResult>& results,
    std::string& error)
{
    try
    {
        pool.wait();
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
    if (externallyCancelled.load(std::memory_order_relaxed))
    {
        error = "callable analysis cancelled before completing the wave";
        results.clear();
        return false;
    }
    return publishCompletedSlots(slots, results, error);
}

bool runParallelWave(
    std::span<const std::size_t> callables,
    std::size_t waveOrdinal,
    std::size_t workerCount,
    const ProjectCallableWaveScheduler::Analyze& analyze,
    const ProjectCallableWaveScheduler::CancellationCheck& cancellationCheck,
    std::vector<ProjectCallableAnalysisResult>& results,
    std::string& error)
{
    try
    {
        driver::CompilationWorkerPool pool(workerCount);
        std::vector<ResultSlot> slots(callables.size());
        std::atomic_bool externallyCancelled = false;
        for (std::size_t index = 0; index < callables.size(); ++index)
        {
            pool.submit([&, index](std::size_t) {
                if (cancellationRequested(cancellationCheck))
                {
                    externallyCancelled.store(true, std::memory_order_relaxed);
                    pool.requestCancellation();
                    return;
                }
                slots[index].emplace(analyze(callables[index], waveOrdinal));
            });
        }
        return waitForWave(pool, externallyCancelled, slots, results, error);
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
}

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
    if (workerCount_ == 1 || callables.size() < 2)
        return runSynchronousWave(
            callables, waveOrdinal, analyze, cancellationCheck, results, error);

    return runParallelWave(
        callables, waveOrdinal, workerCount_, analyze,
        cancellationCheck, results, error);
}

std::size_t ProjectCallableWaveScheduler::requestedWorkerCount() const noexcept
{
    return workerCount_;
}
}
