#pragma once

#include "nativesharp/Frontend/ProjectCallableAnalysisResult.h"

#include <atomic>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::driver
{
class CompilationWorkerPool;
}

namespace nativesharp::frontend
{
class ProjectCallableWaveScheduler final
{
public:
    // Analyze may run concurrently for different stable slots. It must be reentrant and may
    // mutate only state owned by the returned isolated result.
    using Analyze = std::function<ProjectCallableAnalysisResult(
        std::size_t callableOrdinal,
        std::size_t waveOrdinal)>;
    using CancellationCheck = std::function<bool()>;

    explicit ProjectCallableWaveScheduler(std::size_t workerCount) noexcept;
    ~ProjectCallableWaveScheduler();

    ProjectCallableWaveScheduler(const ProjectCallableWaveScheduler&) = delete;
    ProjectCallableWaveScheduler& operator=(const ProjectCallableWaveScheduler&) = delete;

    [[nodiscard]] bool run(
        std::span<const std::size_t> callables,
        std::size_t waveOrdinal,
        const Analyze& analyze,
        const CancellationCheck& cancellationCheck,
        std::vector<ProjectCallableAnalysisResult>& results,
        std::string& error);

    [[nodiscard]] std::size_t requestedWorkerCount() const noexcept;

private:
    [[nodiscard]] bool runExclusive(
        std::span<const std::size_t> callables,
        std::size_t waveOrdinal,
        const Analyze& analyze,
        const CancellationCheck& cancellationCheck,
        std::vector<ProjectCallableAnalysisResult>& results,
        std::string& error);

    std::size_t workerCount_ = 1;
    std::unique_ptr<driver::CompilationWorkerPool> pool_;
    std::exception_ptr poolInitializationFailure_;
    std::atomic_bool running_ = false;
};
}
