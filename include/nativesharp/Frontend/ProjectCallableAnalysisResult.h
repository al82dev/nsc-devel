#pragma once

#include "nativesharp/Diagnostics/DiagnosticEngine.h"

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace nativesharp::frontend
{
class ProjectMethodBodyModel;

class ProjectCallableAnalysisResult final
{
public:
    ProjectCallableAnalysisResult(
        std::size_t callableOrdinal,
        std::size_t waveOrdinal,
        std::unique_ptr<ProjectMethodBodyModel> shard,
        diagnostics::DiagnosticEngine diagnostics) noexcept;
    ~ProjectCallableAnalysisResult();

    ProjectCallableAnalysisResult(ProjectCallableAnalysisResult&&) noexcept;
    ProjectCallableAnalysisResult& operator=(ProjectCallableAnalysisResult&&) noexcept;
    ProjectCallableAnalysisResult(const ProjectCallableAnalysisResult&) = delete;
    ProjectCallableAnalysisResult& operator=(const ProjectCallableAnalysisResult&) = delete;

    [[nodiscard]] std::size_t callableOrdinal() const noexcept;
    [[nodiscard]] std::size_t waveOrdinal() const noexcept;
    [[nodiscard]] ProjectMethodBodyModel& shard() noexcept;
    [[nodiscard]] const ProjectMethodBodyModel& shard() const noexcept;
    [[nodiscard]] diagnostics::DiagnosticEngine& diagnostics() noexcept;
    [[nodiscard]] const diagnostics::DiagnosticEngine& diagnostics() const noexcept;
    void addDiscoveredCallable(std::size_t callableOrdinal);
    void normalizeDiscoveredCallables();
    [[nodiscard]] std::span<const std::size_t> discoveredCallables() const noexcept;

private:
    std::size_t callableOrdinal_ = 0;
    std::size_t waveOrdinal_ = 0;
    std::unique_ptr<ProjectMethodBodyModel> shard_;
    diagnostics::DiagnosticEngine diagnostics_;
    std::vector<std::size_t> discoveredCallables_;
};
}
