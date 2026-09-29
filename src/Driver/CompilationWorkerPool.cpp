#include "nativesharp/Driver/CompilationWorkerPool.h"

#include <stdexcept>
#include <utility>

namespace nativesharp::driver
{
namespace
{
thread_local std::size_t activeWorkerIndex = kNoCompilationWorker;

CompilationJobPolicy resolvedPolicy(std::size_t requestedWorkers)
{
    CompilationJobPolicy policy = activeCompilationJobPolicy() != nullptr
        ? *activeCompilationJobPolicy()
        : CompilationJobPolicy();
    if (requestedWorkers != 0) policy.setRequestedJobs(requestedWorkers);
    return policy;
}
}

CompilationWorkerPool::CompilationWorkerPool(std::size_t workerCount)
    : CompilationWorkerPool(workerCount, {})
{
}

CompilationWorkerPool::CompilationWorkerPool(
    std::size_t workerCount, WorkerInitializer workerInitializer)
    : policy_(resolvedPolicy(workerCount)), workerInitializer_(std::move(workerInitializer))
{
    const std::size_t count = policy_.effectiveJobs();
    workers_.reserve(count);
    try
    {
        for (std::size_t index = 0; index < count; ++index)
        {
            workers_.emplace_back([this, index] { workerMain(index); });
        }
    }
    catch (...)
    {
        {
            const std::lock_guard lock(mutex_);
            stopping_ = true;
        }
        workAvailable_.notify_all();
        for (std::thread& worker : workers_) if (worker.joinable()) worker.join();
        throw;
    }
}

CompilationWorkerPool::~CompilationWorkerPool()
{
    {
        const std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    workAvailable_.notify_all();
    for (std::thread& worker : workers_) if (worker.joinable()) worker.join();
}

std::size_t CompilationWorkerPool::submit(Task task)
{
    if (!task) throw std::invalid_argument("compilation worker task is empty");
    std::size_t ordinal = 0;
    {
        const std::lock_guard lock(mutex_);
        if (stopping_) throw std::logic_error("compilation worker pool is stopping");
        ordinal = nextOrdinal_++;
        if (cancellationRequested_)
        {
            ++discardedTasks_;
            return ordinal;
        }
        queue_.push_back(QueuedTask{ordinal, std::move(task)});
    }
    workAvailable_.notify_one();
    return ordinal;
}

void CompilationWorkerPool::requestCancellation() noexcept
{
    {
        const std::lock_guard lock(mutex_);
        if (cancellationRequested_) return;
        cancellationRequested_ = true;
        discardedTasks_ += queue_.size();
        queue_.clear();
        if (activeTasks_ == 0) idle_.notify_all();
    }
    workAvailable_.notify_all();
}

void CompilationWorkerPool::wait()
{
    std::exception_ptr failure;
    {
        std::unique_lock lock(mutex_);
        idle_.wait(lock, [this] { return queue_.empty() && activeTasks_ == 0; });
        if (!failures_.empty())
        {
            failure = failures_.begin()->second;
            failures_.clear();
        }
        cancellationRequested_ = false;
    }
    if (failure != nullptr) std::rethrow_exception(failure);
}

std::size_t CompilationWorkerPool::workerCount() const noexcept
{
    return workers_.size();
}

bool CompilationWorkerPool::cancellationRequested() const noexcept
{
    const std::lock_guard lock(mutex_);
    return cancellationRequested_;
}

std::size_t CompilationWorkerPool::discardedTaskCount() const noexcept
{
    const std::lock_guard lock(mutex_);
    return discardedTasks_;
}

void CompilationWorkerPool::workerMain(std::size_t workerIndex) noexcept
{
    CompilationJobPolicyScope policyScope(policy_);
    activeWorkerIndex = workerIndex;
    if (workerInitializer_) workerInitializer_(workerIndex);
    for (;;)
    {
        QueuedTask task;
        {
            std::unique_lock lock(mutex_);
            workAvailable_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (stopping_ && queue_.empty()) break;
            task = std::move(queue_.front());
            queue_.pop_front();
            ++activeTasks_;
        }
        try
        {
            task.callback(workerIndex);
        }
        catch (...)
        {
            const std::lock_guard lock(mutex_);
            failures_.emplace(task.ordinal, std::current_exception());
            if (!cancellationRequested_)
            {
                cancellationRequested_ = true;
                discardedTasks_ += queue_.size();
                queue_.clear();
            }
        }
        {
            const std::lock_guard lock(mutex_);
            --activeTasks_;
            if (queue_.empty() && activeTasks_ == 0) idle_.notify_all();
        }
    }
    activeWorkerIndex = kNoCompilationWorker;
}

std::size_t currentCompilationWorkerIndex() noexcept
{
    return activeWorkerIndex;
}
}
