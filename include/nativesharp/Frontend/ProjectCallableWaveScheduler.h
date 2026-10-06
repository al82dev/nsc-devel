#pragma once

#include "nativesharp/Frontend/ProjectCallableAnalysisResult.h"

#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
class ProjectCallableWaveScheduler final
{
public:
    using Analyze = std::function<ProjectCallableAnalysisResult(
        std::size_t callableOrdinal,
        std::size_t waveOrdinal)>;
    using CancellationCheck = std::function<bool()>;

    explicit ProjectCallableWaveScheduler(std::size_t workerCount) noexcept;

    [[nodiscard]] bool run(
        std::span<const std::size_t> callables,
        std::size_t waveOrdinal,
        const Analyze& analyze,
        const CancellationCheck& cancellationCheck,
        std::vector<ProjectCallableAnalysisResult>& results,
        std::string& error) const;

    [[nodiscard]] std::size_t requestedWorkerCount() const noexcept;

private:
    std::size_t workerCount_ = 1;
};
}
