#pragma once

#include "nativesharp/Driver/CompilationJobPolicy.h"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace nativesharp::driver
{
inline constexpr std::size_t kNoCompilationWorker = static_cast<std::size_t>(-1);

class CompilationWorkerPool final
{
public:
    using Task = std::function<void(std::size_t workerIndex)>;
    using WorkerInitializer = std::function<void(std::size_t workerIndex)>;

    // A zero request resolves through the active job policy, or the host default policy when the
    // pool is used outside a Driver invocation.
    explicit CompilationWorkerPool(std::size_t workerCount = 0);
    CompilationWorkerPool(std::size_t workerCount, WorkerInitializer workerInitializer);
    ~CompilationWorkerPool();

    CompilationWorkerPool(const CompilationWorkerPool&) = delete;
    CompilationWorkerPool& operator=(const CompilationWorkerPool&) = delete;

    std::size_t submit(Task task);
    void requestCancellation() noexcept;
    void wait();
    std::size_t workerCount() const noexcept;
    bool cancellationRequested() const noexcept;
    std::size_t discardedTaskCount() const noexcept;

private:
    struct QueuedTask final
    {
        std::size_t ordinal;
        Task callback;
    };

    void workerMain(std::size_t workerIndex) noexcept;

    CompilationJobPolicy policy_;
    WorkerInitializer workerInitializer_;
    std::vector<std::thread> workers_;
    std::deque<QueuedTask> queue_;
    std::map<std::size_t, std::exception_ptr> failures_;
    mutable std::mutex mutex_;
    std::condition_variable workAvailable_;
    std::condition_variable idle_;
    std::size_t nextOrdinal_ = 0;
    std::size_t activeTasks_ = 0;
    std::size_t discardedTasks_ = 0;
    bool cancellationRequested_ = false;
    bool barrierActive_ = false;
    bool stopping_ = false;
};

std::size_t currentCompilationWorkerIndex() noexcept;
}
