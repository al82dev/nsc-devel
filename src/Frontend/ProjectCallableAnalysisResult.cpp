#include "nativesharp/Frontend/ProjectCallableAnalysisResult.h"

#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include <algorithm>
#include <utility>

namespace nativesharp::frontend
{
ProjectCallableAnalysisResult::ProjectCallableAnalysisResult(
    std::size_t callableOrdinal,
    std::size_t waveOrdinal,
    std::unique_ptr<ProjectMethodBodyModel> shard,
    diagnostics::DiagnosticEngine diagnostics) noexcept
    : callableOrdinal_(callableOrdinal),
      waveOrdinal_(waveOrdinal),
      shard_(std::move(shard)),
      diagnostics_(std::move(diagnostics))
{
}

ProjectCallableAnalysisResult::~ProjectCallableAnalysisResult() = default;
ProjectCallableAnalysisResult::ProjectCallableAnalysisResult(
    ProjectCallableAnalysisResult&&) noexcept = default;
ProjectCallableAnalysisResult& ProjectCallableAnalysisResult::operator=(
    ProjectCallableAnalysisResult&&) noexcept = default;

std::size_t ProjectCallableAnalysisResult::callableOrdinal() const noexcept
{
    return callableOrdinal_;
}

std::size_t ProjectCallableAnalysisResult::waveOrdinal() const noexcept
{
    return waveOrdinal_;
}

ProjectMethodBodyModel& ProjectCallableAnalysisResult::shard() noexcept
{
    return *shard_;
}

const ProjectMethodBodyModel& ProjectCallableAnalysisResult::shard() const noexcept
{
    return *shard_;
}

diagnostics::DiagnosticEngine& ProjectCallableAnalysisResult::diagnostics() noexcept
{
    return diagnostics_;
}

const diagnostics::DiagnosticEngine& ProjectCallableAnalysisResult::diagnostics() const noexcept
{
    return diagnostics_;
}

void ProjectCallableAnalysisResult::addDiscoveredCallable(std::size_t callableOrdinal)
{
    discoveredCallables_.push_back(callableOrdinal);
}

void ProjectCallableAnalysisResult::normalizeDiscoveredCallables()
{
    std::ranges::sort(discoveredCallables_);
    discoveredCallables_.erase(
        std::unique(discoveredCallables_.begin(), discoveredCallables_.end()),
        discoveredCallables_.end());
}

std::span<const std::size_t> ProjectCallableAnalysisResult::discoveredCallables() const noexcept
{
    return discoveredCallables_;
}

void ProjectCallableAnalysisResult::releaseShard() noexcept
{
    shard_.reset();
}
}
