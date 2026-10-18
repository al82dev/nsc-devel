#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::frontend
{
class ProjectOopMirModel;
}

namespace nativesharp::llvm_backend
{
enum class ProjectOopCodePartitionPolicy
{
    Balanced,
    StableIncremental,
    CacheAwareBalanced
};

struct ProjectOopCodeUnit final
{
    std::size_t ordinal = 0;
    std::string stableKey;
    // NSC305-L01. Bounded historical cost (a capped moving average of previous builds' observed
    // costs), so a hot unit's weight saturates instead of growing unbounded.
    std::uint64_t historicalCost = 0;
    std::uint64_t estimatedCost = 0;
    std::vector<std::size_t> functionOrdinals;
    std::vector<std::size_t> declarationOrdinals;
};

class ProjectOopCodePartition final
{
public:
    bool build(
        const frontend::ProjectOopMirModel& mirModel,
        std::size_t requestedUnitCount,
        std::string& error,
        ProjectOopCodePartitionPolicy policy = ProjectOopCodePartitionPolicy::Balanced,
        std::string_view cachedPlan = {},
        std::span<const std::uint8_t> reachableDefinitions = {});

    // NSC305-L05. The key excludes bodies so a body-only edit can restore the same balanced plan.
    static std::string persistentPlanKey(
        const frontend::ProjectOopMirModel& mirModel,
        std::size_t requestedUnitCount,
        std::span<const std::uint8_t> reachableDefinitions = {});

    std::span<const ProjectOopCodeUnit> units() const noexcept;
    std::optional<std::size_t> unitForFunction(std::size_t functionOrdinal) const noexcept;
    // NSC305-L01. Records the observed (real, measured) cost of a code unit into its bounded
    // historical weight, saturating at the cap.
    void recordCodeUnitCost(std::size_t ordinal, std::uint64_t observedCost) noexcept;
    std::string persistentPlanPayload() const;
    std::string dump() const;
    bool isBuilt() const noexcept;

private:
    std::vector<ProjectOopCodeUnit> units_;
    std::vector<std::optional<std::size_t>> unitByFunction_;
    std::vector<std::size_t> persistentFunctionOrdinals_;
    std::size_t persistentUnitCount_ = 0;
    bool persistentPlanReady_ = false;
    bool built_ = false;
};
}
